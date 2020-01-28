/*******************************************************************************
 * Copyright 2017-2020 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include <iomanip>

#include "tensorflow/core/common_runtime/optimization_registry.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/graph/graph.h"

#include "logging/ngraph_log.h"
#include "logging/tf_graph_writer.h"
#include "ngraph_bridge/ngraph_api.h"
#include "ngraph_bridge/ngraph_assign_clusters.h"
#include "ngraph_bridge/ngraph_backend_manager.h"
#include "ngraph_bridge/ngraph_capture_variables.h"
#include "ngraph_bridge/ngraph_cluster_manager.h"
#include "ngraph_bridge/ngraph_deassign_clusters.h"
#include "ngraph_bridge/ngraph_encapsulate_clusters.h"
#include "ngraph_bridge/ngraph_enter_prefetch_in_catalog.h"
#include "ngraph_bridge/ngraph_mark_for_clustering.h"
#include "ngraph_bridge/ngraph_rewrite_for_tracking.h"
#include "ngraph_bridge/ngraph_utils.h"

#if defined NGRAPH_DISTRIBUTED
#include "ngraph/distributed.hpp"
#endif

using namespace std;

namespace tensorflow {

namespace ngraph_bridge {

class NGraphRewritePass : public GraphOptimizationPass {
 public:
  virtual Status Run(const GraphOptimizationPassOptions& options) = 0;

 protected:
  // Returns a fresh "serial number" to avoid filename collisions in the graph
  // dumps.
  static int FreshIndex() {
    mutex_lock l(s_serial_counter_mutex);
    return s_serial_counter++;
  }

  static int s_serial_counter GUARDED_BY(s_serial_counter_mutex);
  static mutex s_serial_counter_mutex;
};

int NGraphRewritePass::s_serial_counter = 0;
mutex NGraphRewritePass::s_serial_counter_mutex;

//
// The variable capture pass replaces all instances of VariableV2 with the
// NGraphVariable op. Making this replacement allows us to substitute in a
// kernel that tracks the freshness of variables (invalidating freshness when
// the reference is handed off to an "untrusted" op).
//
class NGraphVariableCapturePass : public NGraphRewritePass {
 public:
  Status Run(const GraphOptimizationPassOptions& options) override {
    // If we don't get a main graph, log that fact and bail.
    if (options.graph == nullptr) {
      NGRAPH_VLOG(0) << "NGraphVariableCapturePass: options.graph == nullptr";
      return Status::OK();
    }

    // For filename generation purposes, grab a fresh index. This is just an
    // arbitrary integer to avoid filename collisions resulting from subsequent
    // runs of this pass.
    int idx = FreshIndex();

    // If requested, dump pre-capture graphs.
    if (DumpPrecaptureGraphs()) {
      DumpGraphs(options, idx, "precapture", "Pre-Capture Graph");
    }

    // If ngraph is disabled via ngraph_bridge api or NGRAPH_TF_DISABLE is set
    // we will not do anything; all subsequent
    // passes become a no-op.
    bool ngraph_not_enabled =
        (!config::IsEnabled()) || (std::getenv("NGRAPH_TF_DISABLE") != nullptr);
    bool already_processed = IsProcessedByNgraphPass(options.graph->get());
    if (!already_processed && ngraph_not_enabled) {
      NGRAPH_VLOG(0) << "NGraph is available but disabled.";
    }
    if (ngraph_not_enabled || already_processed) {
      // In the case that we run a network with ngraph, cluster manager gets
      // populated. Then we run a new network, it repopulates the cluster
      // manager. This works under the assumption that whenever
      // NGraphEncapsulate's Compute is run the rewrite passes (grappler or
      // optimization passes) have also run (compute --> rewrite). Now that
      // assumption is broken because now we support NGraphEncapsulate enabled
      // graphs. Such graphs will not run the first rewrite pass, hence the
      // cluster manager is not overwritten. Which would mean that cluster
      // manager contains stale data from a previous run. Hence evicting cluster
      // manager when rewrite passes are not run.
      NGRAPH_VLOG(1) << std::string("Rewrite pass will not run because ") +
                            (already_processed ? "graph is already preprocessed"
                                               : "ngraph is disabled");
      NGraphClusterManager::EvictAllClusters();
      return Status::OK();
    }

    // Do variable capture then, if requested, dump the graphs.
    std::set<string> skip_these_nodes = {};
    TF_RETURN_IF_ERROR(
        CaptureVariables(options.graph->get(), skip_these_nodes));
    if (DumpCapturedGraphs()) {
      DumpGraphs(options, idx, "captured", "Graph With Variables Captured");
    }

    return Status::OK();
  }
};

//
// Pass that rewrites the graph for nGraph operation.
//
// The pass has several phases, each executed in sequence:
//
//   1. Marking [ngraph_mark_for_clustering.cc]
//   2. Cluster Assignment [ngraph_assign_clusters.cc]
//   3. Cluster Deassignment [ngraph_deassign_clusters.cc]
//   4. Cluster Encapsulation [ngraph_encapsulate_clusters.cc]
//   5. Rewrite Variable Type Ops for Tracking [ngraph_rewrite_for_tracking.cc]
//   6. Enter In Catalog  [ngraph_enter_in_catalog.cc]
//
// Between phases, graph dumps (in both .dot and .pbtxt format) may be
// requested by setting the following environment variables:
//
//   NGRAPH_TF_DUMP_UNMARKED_GRAPHS=1      dumps graphs before phase 1
//   NGRAPH_TF_DUMP_MARKED_GRAPHS=1        dumps graphs after phase 1
//   NGRAPH_TF_DUMP_CLUSTERED_GRAPHS=1     dumps graphs after phase 2
//   NGRAPH_TF_DUMP_DECLUSTERED_GRAPHS=1   dumps graphs after phase 3
//   NGRAPH_TF_DUMP_ENCAPSULATED_GRAPHS=1  dumps graphs after phase 4
//   NGRAPH_TF_DUMP_TRACKED_GRAPHS=1       dumps graphs after phase 5
//   NGRAPH_TF_DUMP_CATALOGED_GRAPHS=1     dumps graphs after phase 6
//   NGRAPH_TF_DUMP_GRAPHS=1               all of the above
//
class NGraphEncapsulationPass : public NGraphRewritePass {
 public:
  Status Run(const GraphOptimizationPassOptions& options) override {
    // If we don't get a main graph, log that fact and bail.
    if (options.graph == nullptr) {
      NGRAPH_VLOG(0) << "NGraphEncapsulationPass: options.graph == nullptr";
      return Status::OK();
    }

    // For filename generation purposes, grab a fresh index. This is just an
    // arbitrary integer to avoid filename collisions resulting from subsequent
    // runs of this pass.
    int idx = FreshIndex();

    // If requested, dump unmarked graphs.
    if (DumpUnmarkedGraphs()) {
      DumpGraphs(options, idx, "unmarked", "Unmarked Graph");
    }

    // If ngraph is disabled via ngraph_bridge api or NGRAPH_TF_DISABLE is set
    // we will not do anything; all subsequent
    // passes become a no-op.
    bool ngraph_not_enabled =
        (!config::IsEnabled()) || (std::getenv("NGRAPH_TF_DISABLE") != nullptr);
    bool already_processed = IsProcessedByNgraphPass(options.graph->get());
    if (!already_processed && ngraph_not_enabled) {
      NGRAPH_VLOG(0) << "NGraph is available but disabled.";
    }
    if (ngraph_not_enabled || already_processed) {
      NGRAPH_VLOG(1) << std::string("Rewrite pass will not run because ") +
                            (already_processed ? "graph is already preprocessed"
                                               : "ngraph is disabled");
      NGraphClusterManager::EvictAllClusters();
      return Status::OK();
    }

    // Get backend + its configurations
    // to be attached to the nodes
    // Precedence Order: Env Variable > BackendManager
    std::unordered_map<std::string, std::string> config_map;
    string backend_creation_string;
    // GetCurrentlySetBackendName could return GPU:0 (not just GPU)
    TF_RETURN_IF_ERROR(
        BackendManager::GetCurrentlySetBackendName(&backend_creation_string));

    // splits into {"ngraph_backend", "ngraph_device_id"}
    config_map = BackendManager::GetBackendAttributeValues(
        backend_creation_string);  // SplitBackendConfig
    config_map.erase("ngraph_backend");

    if ((std::getenv("NGRAPH_TF_LOG_0_DISABLED") == nullptr)) {
      NGRAPH_VLOG(0) << "NGraph using backend: " << backend_creation_string;
    }

    // Now Process the Graph

    // 1. Mark for clustering then, if requested, dump the graphs.
    std::set<string> skip_these_nodes = {};
    TF_RETURN_IF_ERROR(MarkForClustering(options.graph->get(), skip_these_nodes,
                                         backend_creation_string));
    if (DumpMarkedGraphs()) {
      DumpGraphs(options, idx, "marked", "Graph Marked for Clustering");
    }

    // 2. Assign clusters then, if requested, dump the graphs.
    TF_RETURN_IF_ERROR(AssignClusters(options.graph->get()));
    if (DumpClusteredGraphs()) {
      DumpGraphs(options, idx, "clustered", "Graph with Clusters Assigned");
    }

    // 3. Deassign trivial clusters then, if requested, dump the graphs.
    TF_RETURN_IF_ERROR(DeassignClusters(options.graph->get()));
    if (DumpDeclusteredGraphs()) {
      DumpGraphs(options, idx, "declustered",
                 "Graph with Trivial Clusters De-Assigned");
    }

    // 4. Encapsulate clusters then, if requested, dump the graphs.
    FunctionDefLibrary* fdeflib_new = new FunctionDefLibrary();
    auto status = EncapsulateClusters(options.graph->get(), idx, fdeflib_new,
                                      config_map, {0, {}});
    if (status != Status::OK()) {
      delete (fdeflib_new);
      return status;
    }
    // TODO: not using fdeflib_new in this path. Only grappler path uses it
    delete (fdeflib_new);
    if (DumpEncapsulatedGraphs()) {
      DumpGraphs(options, idx, "encapsulated",
                 "Graph with Clusters Encapsulated");
    }

    // 5. Rewrite for tracking then, if requested, dump the graphs.
    TF_RETURN_IF_ERROR(RewriteForTracking(options.graph->get(), idx));
    if (DumpTrackedGraphs()) {
      DumpGraphs(options, idx, "tracked",
                 "Graph with Variables Rewritten for Tracking");
    }

    // 6. Enter in catalog then.
    TF_RETURN_IF_ERROR(EnterPrefetchInCatalog(options.graph->get(), idx));
    if (DumpCatalogedGraphs()) {
      DumpGraphs(options, idx, "prefetch-cataloged",
                 "Graph with Prefetched Inputs Entered in Catalog");
    }

    return Status::OK();
  }
};

}  // namespace ngraph_bridge

REGISTER_OPTIMIZATION(OptimizationPassRegistry::POST_PLACEMENT, 0,
                      ngraph_bridge::NGraphVariableCapturePass);
REGISTER_OPTIMIZATION(OptimizationPassRegistry::POST_REWRITE_FOR_EXEC, 0,
                      ngraph_bridge::NGraphEncapsulationPass);
}  // namespace tensorflow