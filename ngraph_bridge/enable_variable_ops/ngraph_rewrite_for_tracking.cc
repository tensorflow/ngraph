/*******************************************************************************
 * Copyright 2017-2019 Intel Corporation
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
#include "tensorflow/core/graph/types.h"

#include "ngraph_bridge/enable_variable_ops/ngraph_replace_op_utilities.h"
#include "ngraph_bridge/ngraph_rewrite_for_tracking.h"
#include "ngraph_bridge/ngraph_utils.h"

using namespace std;

namespace tensorflow {

namespace ngraph_bridge {

//
// Main entry point for rewrite-for-tracking.
//
Status RewriteForTracking(Graph* graph, int graph_id) {
  const static std::map<
      const string,
      const function<Status(
          Graph * graph, Node * node, Node * *replacement,
          const string replacement_node_name, const string replacement_op_type,
          const bool just_looking, const bool is_tf_just_looking,
          const bool outputs_ng_supported, const int graph_id,
          const bool is_backend_set)>>
      REWRITE_REPLACE_OP_MAP{{"NGraphAssign", ReplaceAssign},
                             {"NGraphVariable", ReplaceVariable}};

  std::vector<Node*> replaced_nodes;
  for (auto node : graph->op_nodes()) {
    auto itr = REWRITE_REPLACE_OP_MAP.find(node->type_string());
    if (itr != REWRITE_REPLACE_OP_MAP.end()) {
      NGRAPH_VLOG(1) << "Checking: " << DebugNode(node) << " " << node->name();

      bool is_tf_just_looking = true;
      bool outputs_ng_supported = true;
      bool just_looking = true;

      // Check if all the outputs of this node are supported by nGraph
      for (auto edge : node->out_edges()) {
        auto dst = edge->dst();
        NGRAPH_VLOG(1) << "dst node " << DebugNode(dst);
        if (dst->IsOp() && !edge->IsControlEdge() &&
            !IsNGSupportedType(dst->type_string())) {
          NGRAPH_VLOG(1) << "ngraph does not support dst node ";
          outputs_ng_supported = false;
          break;
        }
      }

      // If any of the nodes reading from this Variable node read the data as
      // reference then we dont track it, else we do
      for (auto edge : node->out_edges()) {
        if (edge->dst()->IsOp() && !edge->IsControlEdge() &&
            IsRefType(edge->dst()->input_type(edge->dst_input()))) {
          just_looking = false;
          // if the output reference is read by NGraph supported ops, do not
          // turn off is_tf_just_looking
          if (!IsNGVariableType(edge->dst()->type_string())) {
            NGRAPH_VLOG(1)
                << DebugNode(edge->dst())
                << "needs reference, setting is_tf_just_looking to false";
            is_tf_just_looking = false;

            // Since the dst node takes in this variable as a reference 
            // and is not supported by NGraph, it might update the 
            // variable hence the sync node is required here.
            NodeBuilder::NodeOut input_ref;
            input_ref = NodeBuilder::NodeOut(edge->src(), edge->src_output());
            DataType dtype;
            TF_RETURN_IF_ERROR(GetNodeAttr(node->attrs(), "dtype", &dtype));
            string shared_name;
            TF_RETURN_IF_ERROR(GetNodeAttr(node->attrs(), "shared_name", &shared_name));
            Node* sync_node;
            NodeBuilder nb = NodeBuilder("sync_node","NGraphVariableUpdateNGTensor")
                                .Input(input_ref)
                                .Attr("ngraph_graph_id", graph_id)
                                .Attr("ngraph_variable_shared_name", shared_name)
                                .Attr("T", dtype)
                                .Device(node->assigned_device_name());
            Status status = nb.Finalize(graph, &sync_node);
            TF_RETURN_IF_ERROR(status);

            // Connect ouput edges from the TF optimizer to the sync node
            // This should replace the control edges too ???
            TF_RETURN_IF_ERROR(ReplaceOutputEdges(graph, edge->dst(), sync_node));

            // Add a control edge from the TF optimizer node 
            // to the sync node making sure that the sync node 
            // is executed after the TF optimizer
            graph->AddEdge(edge->dst(), Graph::kControlSlot, sync_node,
                     Graph::kControlSlot);
            break;
          }
        }
      }

      NGRAPH_VLOG(1) << "Is_TF_Just_Looking: " << PrintBool(is_tf_just_looking);
      NGRAPH_VLOG(1) << "Just_Looking: " << PrintBool(just_looking);
      NGRAPH_VLOG(1) << "Outputs supported by nGraph: "
                     << PrintBool(outputs_ng_supported);
      NGRAPH_VLOG(1) << "Requires Replacement "
                     << PrintBool(is_tf_just_looking || !outputs_ng_supported ||
                                  !just_looking);

      std::string node_new_name = node->name();
      if (just_looking) {
        node_new_name += "/peek";
      }

      if (is_tf_just_looking) {
        node_new_name += "/tf_just_looking";
      }

      if (!outputs_ng_supported) {
        node_new_name += "/non_ng_outputs";
      }

      node_new_name += "/gid_" + to_string(graph_id);
      NGRAPH_VLOG(1) << "Replacing " << node->name() << " New Node name "
                     << node_new_name;

      Node* replacement;

      // Create and add the replacement node
      TF_RETURN_IF_ERROR((itr->second)(graph, node, &replacement, node_new_name,
                                       node->type_string(), just_looking,
                                       is_tf_just_looking, outputs_ng_supported,
                                       graph_id, true));

      TF_RETURN_IF_ERROR(ReplaceInputControlEdges(graph, node, replacement));
      TF_RETURN_IF_ERROR(ReplaceOutputEdges(graph, node, replacement));

      replaced_nodes.push_back(node);

    }  // end of checking if it is NGVariableType
  }    // end of looping through the nodes in the graph
  for (auto node : replaced_nodes) {
    graph->RemoveNode(node);
  }

  return Status::OK();
}

}  // namespace ngraph_bridge

}  // namespace tensorflow
