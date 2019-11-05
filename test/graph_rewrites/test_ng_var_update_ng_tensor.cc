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

#include <vector>

#include "gtest/gtest.h"
#include "tensorflow/core/framework/allocator.h"
#include "tensorflow/core/framework/node_def_builder.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/resource_mgr.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/kernels/ops_testutil.h"
#include "tensorflow/core/kernels/ops_util.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/framework/node_def_builder.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/graph/node_builder.h"
#include "tensorflow/core/public/session.h"

#include "ngraph_bridge/enable_variable_ops/ngraph_var.h"
#include "ngraph_bridge/enable_variable_ops/ngraph_variable_update_ng_tensor_op.h"
#include "ngraph_bridge/enable_variable_ops/tf_fake_input.h"
#include "ngraph_bridge/ngraph_utils.h"
#include "ngraph_bridge/ngraph_encapsulate_op.h"
#include "ngraph_bridge/ngraph_rewrite_for_tracking.h"
#include "test/test_utilities.h"
#include "logging/tf_graph_writer.h"

namespace tensorflow {
namespace ngraph_bridge {
namespace testing {

class NGVarUpdateNGTensorOpTest : public tensorflow::OpsTestBase {
 protected:
  void MakeOp() {
    ASSERT_OK(NodeDefBuilder("sync_node", "NGraphVariableUpdateNGTensor")
                  .Input(FakeInput(DT_FLOAT_REF))
                  .Attr("T", DT_FLOAT)
                  .Attr("ngraph_variable_shared_name", "var1")
                  .Attr("ngraph_graph_id", 1)
                  .Finalize(node_def()));
    ASSERT_OK(InitOp());
  }
};

// This test requires the env variable NGRAPH_TF_NGVARIABLE_BUFFER_SHARING=0
// when running on CPU
TEST_F(NGVarUpdateNGTensorOpTest, KernelTest) {
  list<string> env_vars{"NGRAPH_TF_NGVARIABLE_BUFFER_SHARING"};
  const unordered_map<string, string>& env_map = StoreEnv(env_vars);
  SetEnvVariable("NGRAPH_TF_NGVARIABLE_BUFFER_SHARING", "0");

  // Create a normal TF tensor: input_tf_tensor and assign values
  // This will be used to assign initial value to the TF tensor
  // that is a part of the NGraph Var resource
  Tensor input_tf_tensor(DT_FLOAT, TensorShape({2}));
  auto input_tf_tensor_flat = input_tf_tensor.flat<float>();
  for (size_t i = 0; i < input_tf_tensor_flat.size(); i++) {
    input_tf_tensor_flat.data()[i] = 1.0;
  }

  // Create an NGraphVar resource object
  NGraphVar* var = new NGraphVar(DT_FLOAT, TensorShape{2}, "CPU");

  // Assign the ng_tensor with initial value and use it to copy an
  // initial value to the tf_tensor
  // After this, both ng_tensor and tf_tensor will hold the same value
  // i.e. (1.0, 1.0)
  var->update_ng_tensor(&input_tf_tensor);
  var->copy_ng_to_tf();

  // Create a normal TF tensor: input_ng_tensor and assign values
  // This will be used to assign initial value to the NG tensor
  // that is a part of the NGraph Var resource
  Tensor input_ng_tensor(DT_FLOAT, TensorShape({2}));
  auto input_ng_tensor_flat = input_ng_tensor.flat<float>();
  // Assign a new value to the normal TF tensor: input_tensor
  for (size_t i = 0; i < input_ng_tensor_flat.size(); i++) {
    input_ng_tensor_flat.data()[i] = 5.0;
  }

  // Update the ng_tensor with the new value i.e. (5.0, 5.0)
  var->update_ng_tensor(&input_ng_tensor);
  // So now, both ng_tensor and tf_tensor have different values
  // which is the desired configuration for the test

  // Create NGraphVariableUpdateNGTensor node
  MakeOp();

  // Add NGraph resource to the same container as the test op
  ContainerInfo cinfo_;
  NodeDef ndef;
  ndef.set_name("node1");
  AddNodeAttr("container", "", &ndef);
  AddNodeAttr("shared_name", "var1", &ndef);
  ASSERT_OK(cinfo_.Init(device_->resource_manager(), ndef, true));

  ASSERT_OK(device_->resource_manager()->Create<NGraphVar>(cinfo_.container(),
                                                           cinfo_.name(), var));
  inputs_.push_back({&lock_for_refs_, var->tensor()});

  ASSERT_OK(RunOpKernel());

  shared_ptr<ngraph::runtime::Tensor> ng_t = var->ng_tensor();
  Tensor output_tensor(DT_FLOAT, TensorShape({2}));
  void* dst_ptr = DMAHelper::base(&output_tensor);
  ng_t->read(dst_ptr, 0, output_tensor.TotalBytes());

  Compare(output_tensor, input_tf_tensor, 0);

  UnsetEnvVariable("NGRAPH_TF_NGVARIABLE_BUFFER_SHARING");
  RestoreEnv(env_map);
}

TEST_F(NGVarUpdateNGTensorOpTest, SimpleGraph1) {

  Graph g(OpRegistry::Global());
  PartialTensorShape varShape({2, 2});

  Node* var_node;
  ASSERT_OK(NodeBuilder("var_node", "NGraphVariable")
                .Attr("shape", varShape)
                .Attr("dtype", DT_FLOAT)
                .Attr("just_looking", false)
                .Attr("copy_to_tf", false)
                .Attr("container", "")
                .Attr("shared_name", "node1")
                .Attr("ngraph_graph_id", 1)
                .Attr("_ngraph_backend","CPU")
                .Finalize(&g, &var_node));

  std::vector<DataType> input_types;
  input_types.push_back(DT_FLOAT);
  std::vector<DataType> output_types;
  output_types.push_back(DT_FLOAT);
  std::vector<NodeBuilder::NodeOut> inputs;
  inputs.push_back(NodeBuilder::NodeOut(var_node, 0));
  Node* encap_node;
  ASSERT_OK(NodeBuilder("encap_node", "NGraphEncapsulate")
                .Attr("Targuments", input_types)
                .Attr("Tresults", output_types)
                .Attr("ngraph_cluster", 1)
                .Attr("ngraph_graph_id", 1)
                .Attr("ngraph_backend", "CPU")
                .Attr("ngraph_device_id", "1")
                .Input(inputs)
                .Finalize(&g, &encap_node));

  NodeBuilder::NodeOut input_val = NodeBuilder::NodeOut(encap_node, 0);
  Node* assign;
  ASSERT_OK(NodeBuilder("assign", "Assign")
                .Input(var_node)
                .Input(input_val)
                .Attr("T", DT_FLOAT)
                .Finalize(&g, &assign));

  Node* source = g.source_node();
  Node* sink = g.sink_node();
  g.AddEdge(source, Graph::kControlSlot, var_node, Graph::kControlSlot);
  g.AddEdge(assign, Graph::kControlSlot, sink, Graph::kControlSlot);

  ASSERT_OK(RewriteForTracking(&g, 0));

  map<string, Node*> node_map;
  for (auto node : g.op_nodes()) {
    node_map[node->name()] = node;
  }
  ASSERT_EQ(node_map.find("sync_node")->second->type_string(),
            "NGraphVariableUpdateNGTensor");
  node_map.clear();

}
}  // testing
}  // ngraph_bridge
}  // tensorflow