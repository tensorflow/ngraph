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

#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/graph/node_builder.h"

#include "ngraph_bridge/ngraph_api.h"
#include "ngraph_bridge/ngraph_capture_variables.h"
#include "ngraph_bridge/ngraph_find_replace_prefetchdataset.h"
#include "ngraph_bridge/ngraph_utils.h"

using namespace std;

namespace tensorflow {

namespace ngraph_bridge {

//
// Utility function to check if it is an output node
// Skip capturing it, if yes.
static bool IsOutputNode(const Node* node,
                         const std::set<string> skip_these_nodes) {
  bool found = skip_these_nodes.find(node->name()) != skip_these_nodes.end();
  if (found) {
    NGRAPH_VLOG(5) << "NGTF_OPTIMIZER: Found Output Node: " << node->name()
                   << " - skip capturing it";
  }
  return found;
}

//
// Main entry point for the variable-capture.
//
Status CaptureVariables(Graph* graph, const std::set<string> skip_these_nodes) {
  if (config::IsEnabled() == false) {
    return Status::OK();
  }

  std::vector<Node*> replaced_nodes;
  std::vector<Node*> make_iterator_nodes;
  for (auto node : graph->op_nodes()) {
    if (!IsOutputNode(node, skip_these_nodes)) {
      if (node->type_string() == "VariableV2") {
        NGRAPH_VLOG(4) << "Capturing: " << node->name();

        TensorShape shape;
        DataType dtype;
        TF_RETURN_IF_ERROR(GetNodeAttr(node->attrs(), "shape", &shape));
        TF_RETURN_IF_ERROR(GetNodeAttr(node->attrs(), "dtype", &dtype));

        std::string container;
        std::string shared_name;
        if (GetNodeAttr(node->attrs(), "container", &container) !=
            Status::OK()) {
          container = "";
        }
        if (GetNodeAttr(node->attrs(), "shared_name", &shared_name) !=
            Status::OK()) {
          shared_name = "";
        }

        Node* replacement;

        // TODO(amprocte): Do we need to copy "_" attributes?
        TF_RETURN_IF_ERROR(NodeBuilder(node->name(), "NGraphVariable")
                               .Attr("shape", shape)
                               .Attr("dtype", dtype)
                               .Attr("container", container)
                               .Attr("shared_name", shared_name)
                               .Device(node->assigned_device_name())
                               .Finalize(graph, &replacement));

        replacement->set_assigned_device_name(node->assigned_device_name());

        std::vector<const Edge*> edges;

        // Add edge from the input nodes (to the variable node (VariableV2))
        // to the replacement node (NGraphVariable)
        NGRAPH_VLOG(4) << "Replacing Node " << node->DebugString() << " with "
                       << replacement->DebugString();

        std::vector<const Edge*> edges_to_remove;
        std::vector<std::tuple<Node*, int, Node*, int>> edges_to_add;
        for (auto edge : node->in_edges()) {
          NGRAPH_VLOG(4) << "Replacing: In Edge " << edge->DebugString();
          edges_to_add.push_back(std::tuple<Node*, int, Node*, int>(
              edge->src(), edge->src_output(), replacement, edge->dst_input()));
          edges_to_remove.push_back(edge);
        }

        for (auto edge : node->out_edges()) {
          NGRAPH_VLOG(4) << "Replacing: OutEdge " << edge->DebugString();
          edges_to_add.push_back(std::tuple<Node*, int, Node*, int>(
              replacement, edge->src_output(), edge->dst(), edge->dst_input()));
          edges_to_remove.push_back(edge);
        }

        for (const auto& i : edges_to_add) {
          NGRAPH_VLOG(4) << "Adding: " << get<0>(i) << "  " << get<1>(i) << "  "
                         << get<2>(i) << " " << get<3>(i);
          graph->AddEdge(get<0>(i), get<1>(i), get<2>(i), get<3>(i));
        }

        // Though edges will be removed when we remove the node
        // we specifically remove the edges to be sure
        for (auto edge : edges_to_remove) {
          NGRAPH_VLOG(4) << "Removing: " << edge->DebugString();
          graph->RemoveEdge(edge);
        }

        replaced_nodes.push_back(node);
      } else if (node->type_string() == "MakeIterator") {
        make_iterator_nodes.push_back(node);
      }
    }
  }

  for (auto node : replaced_nodes) {
    NGRAPH_VLOG(4) << "Removing: " << node->name();
    graph->RemoveNode(node);
  }

  // If Prefetch is requested
  if (std::getenv(NGraphPrefetchSharedResouce::NGRAPH_TF_USE_PREFETCH) !=
      nullptr) {
    if (make_iterator_nodes.size() > 1) {
      return errors::Internal(
          "Found more than 1 MakeIterator nodes. This case is not supported.");
    }
    // Else try to capture it
    Node* make_iterator_node = make_iterator_nodes[0];
    // We expect the MakeIterator to have 1 input thats
    // an iterator and the other one can be either a
    // PrefetchDataset node or a ModelDataset node
    // Other cases are not handled at the moment.
    Node* prefetch_node = FindPrefetch(make_iterator_node);
    if (prefetch_node != nullptr) {
      return ReplacePrefetch(graph, prefetch_node);
    } else {
      return errors::Internal(
          "Did not find PrefetchDataset or "
          "ModelDataset+OptimizeDataset+PrefetchDataset as MakeIterator "
          "nodes' inputs. Only those 2 cases are handled for now.");
    }
  }

  make_iterator_nodes.clear();
  replaced_nodes.clear();
  return Status::OK();
}

}  // namespace ngraph_bridge

}  // namespace tensorflow
