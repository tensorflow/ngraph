/*******************************************************************************
 * Copyright 2019 Intel Corporation
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

#include "tensorflow/core/graph/algorithm.h"
#include "tensorflow/core/graph/graph.h"

#include "ngraph/ngraph.hpp"
#include "ngraph/serializer.hpp"

#include "logging/ngraph_log.h"
#include "ngraph_bridge/enable_variable_ops/ngraph_enter_in_catalog.h"
#include "ngraph_bridge/ngraph_catalog.h"
#include "ngraph_bridge/ngraph_utils.h"

using namespace std;
namespace ng = ngraph;

namespace tensorflow {

namespace ngraph_bridge {

// Used by Variable and other Modifier Ops (NGraphVariable, NGraphAssign)
// for accessing the variable object from resource manager using shared
// name
// If the op is not of type NGraphVariable,
//    then recurse over its 1st input till we get reach the variable
// Assumes: the Variable that is being modified is the 1st input and the only
// modifiable input
// If the op has many such inputs, this function needs to be called for each of
// them
// It is bound to terminate as the modifier ops like Assign, AssignAdd,
// ApplyGradientDescent, etc
// always operate on a Variable
Status GetSharedName(Node* node, string* shared_name) {
  if (node->type_string() == "NGraphVariable") {
    TF_RETURN_IF_ERROR(GetNodeAttr(node->attrs(), "shared_name", shared_name));
    if (shared_name->empty()) {
      (*shared_name) = node->name();
    }
    return Status::OK();
  }

  Node* input_0;
  TF_RETURN_IF_ERROR(node->input_node(0, &input_0));
  return GetSharedName(input_0, shared_name);
}

// 1. Populate the NGraphCatalog
//    - input_variable_map
//    - encap_output_info_map_
//    - encap_output_copy_indexes_map_
// 2. Attach Graph Ids to the node

Status EnterInCatalog(Graph* graph, int graph_id) {
  // Topological Sort
  vector<Node*> ordered;
  GetReversePostOrder(*graph, &ordered);

  for (auto node : ordered) {
    if (node->type_string() == "NGraphAssign") {
      Node* input_1;
      TF_RETURN_IF_ERROR(node->input_node(1, &input_1));
      if (input_1->type_string() == "NGraphEncapsulate") {
        NGRAPH_VLOG(4)
            << "Input node type for NGraphAssign is NGraphEncapsulate";
        // attach attribute _ngraph_remove to this NGraphAssign
        node->AddAttr("_ngraph_remove", true);
        // get variable shared_name
        string shared_name;
        TF_RETURN_IF_ERROR(GetSharedName(node, &shared_name));
        // get attribute copy_to_tf
        bool copy_to_tf;
        TF_RETURN_IF_ERROR(
            GetNodeAttr(node->attrs(), "copy_to_tf", &copy_to_tf));
        // populate encap_output_info_map_
        const Edge* edge;
        TF_RETURN_IF_ERROR(node->input_edge(1, &edge));
        int output_index = edge->src_output();
        NGRAPH_VLOG(4) << "output_index " << output_index;
        string key = NGraphCatalog::CreateNodeKey(graph_id, input_1->name(),
                                                  output_index);

        tuple<string, bool> value = make_tuple(shared_name, copy_to_tf);
        NGRAPH_VLOG(4) << "Adding to EncapOutputInfoMap ";
        NGRAPH_VLOG(4) << "Key: " << key;
        NGRAPH_VLOG(4) << "Value: " << get<0>(value) << " " << get<1>(value);
        try {
          NGraphCatalog::AddToEncapOutputInfoMap(key, value);
        } catch (const std::exception& exp) {
          return errors::Internal(
              "Caught exception while entering in catalog: ", exp.what(), "\n");
        }
        // This NGraphAssign will be removed subsequently
        // so we dont need to fill the rest of the catalog
        continue;
      }
    }

    // Update the input variable map
    if (IsNGVariableType(node->type_string())) {
      string node_key = NGraphCatalog::CreateNodeKey(graph_id, node->name(), 0);
      string shared_name;
      TF_RETURN_IF_ERROR(GetSharedName(node, &shared_name));
      try {
        NGraphCatalog::AddToInputVariableSharedNameMap(node_key, shared_name);
      } catch (const std::exception& exp) {
        return errors::Internal("Caught exception while entering in catalog: ",
                                exp.what(), "\n");
      }
      NGRAPH_VLOG(4) << "Adding in InputVariableSharedNameMap ";
      NGRAPH_VLOG(4) << "Key: " << node_key;
      NGRAPH_VLOG(4) << "Value: " << shared_name;
    } else if (node->type_string() == "NGraphEncapsulate") {
      // input catalog
      for (auto edge : node->in_edges()) {
        if (edge->src()->IsOp() && !edge->IsControlEdge() &&
            IsNGVariableType(edge->src()->type_string())) {
          auto src = edge->src();
          string node_key = NGraphCatalog::CreateNodeKey(graph_id, node->name(),
                                                         edge->dst_input());
          string shared_name;
          TF_RETURN_IF_ERROR(GetSharedName(src, &shared_name));
          try {
            NGraphCatalog::AddToInputVariableSharedNameMap(node_key,
                                                           shared_name);
            NGRAPH_VLOG(4) << "Adding in InputVariableSharedNameMap ";
            NGRAPH_VLOG(4) << "Key: " << node_key;
            NGRAPH_VLOG(4) << "Value: " << shared_name;
          } catch (const std::exception& exp) {
            return errors::Internal(
                "Caught exception while entering in catalog: ", exp.what(),
                "\n");
          }
        }
      }

      // output ng-copy map catalog
      unordered_set<int> op_index_to_copy;
      for (auto edge : node->out_edges()) {
        if (edge->dst()->IsOp() && !edge->IsControlEdge() &&
            !IsNGVariableType(edge->dst()->type_string())) {
          NGRAPH_VLOG(4) << "Adding in OutputCopyIndexesMap ";
          NGRAPH_VLOG(4) << "Key: " << node->name();
          NGRAPH_VLOG(4) << "Ouput Index: " << edge->src_output();
          NGRAPH_VLOG(4) << "Required by " << DebugNode(edge->dst());
          op_index_to_copy.insert(edge->src_output());
        }
      }

      try {
        NGraphCatalog::AddToEncapOutputCopyIndexesMap(graph_id, node->name(),
                                                      op_index_to_copy);
      } catch (const std::exception& exp) {
        return errors::Internal("Caught exception while entering in catalog: ",
                                exp.what(), "\n");
      }

    }  // end of node is type NGraphEncapsulate
  }    // enter in catalog

  NGRAPH_VLOG(4) << "Entered in Catalog";
  return Status::OK();
}

}  // namespace ngraph_bridge

}  // namespace tensorflow
