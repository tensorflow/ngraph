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
#include "gtest/gtest.h"

#include "enable_variable_ops/ngraph_replace_op_utilities.h"
#include "ngraph_capture_variables.h"
#include "ngraph_utils.h"
#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/public/session.h"
#include "tf_graph_writer.h"

using namespace std;
namespace ng = ngraph;

namespace tensorflow {

namespace ngraph_bridge {

namespace testing {

#define ASSERT_OK(x) ASSERT_EQ((x), ::tensorflow::Status::OK());

// Test that an Assign attached to a Temporary Variable is not
// captured and replaced by NGraphAssign
TEST(CaptureVariables, TempVar) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto init_value = ops::Const(root, {{2.f, 3.f}, {4.f, 5.f}});

  auto var_x = ops::Variable(root.WithOpName("VarX"), varShape, DT_FLOAT);
  auto var_x_assign =
      ops::Assign(root.WithOpName("AssignX"), var_x, init_value);

  auto var_y =
      ops::TemporaryVariable(root.WithOpName("VarY"), varShape, DT_FLOAT);
  auto var_y_assign =
      ops::Assign(root.WithOpName("AssignY"), var_y, init_value);

  std::set<string> skip_these_nodes = {};

  Graph graph(OpRegistry::Global());
  TF_CHECK_OK(root.ToGraph(&graph));

  ASSERT_OK(CaptureVariables(&graph, skip_these_nodes));

  for (auto node : graph.op_nodes()) {
    auto node_name = node->name();
    if (node_name == "VarX")
      ASSERT_EQ("NGraphVariable", node->type_string());
    else if (node_name == "VarY")
      ASSERT_NE("NGraphVariable", node->type_string());
    else if (node_name == "AssignX")
      ASSERT_EQ("NGraphAssign", node->type_string());
    else if (node_name == "AssignY")
      ASSERT_NE("NGraphAssign", node->type_string());
  }
}

// Test that an Assign with attribute variable_scope = false is not
// captured and replaced by NGraphAssign
TEST(CaptureVariables, VariableScope) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto init_value = ops::Const(root, {{2.f, 3.f}, {4.f, 5.f}});

  auto var_x = ops::Variable(root.WithOpName("VarX"), varShape, DT_FLOAT);
  auto var_x_assign =
      ops::Assign(root.WithOpName("AssignX"), var_x, init_value);

  auto var_y = ops::Variable(root.WithOpName("VarY"), varShape, DT_FLOAT);
  auto attr_validate_shape = ops::Assign::Attrs().ValidateShape(false);
  auto var_y_assign = ops::Assign(root.WithOpName("AssignY"), var_y, init_value,
                                  attr_validate_shape);

  std::set<string> skip_these_nodes = {};

  Graph graph(OpRegistry::Global());
  TF_CHECK_OK(root.ToGraph(&graph));

  ASSERT_OK(CaptureVariables(&graph, skip_these_nodes));

  for (auto node : graph.op_nodes()) {
    auto node_name = node->name();
    if (node_name == "VarX")
      ASSERT_EQ("NGraphVariable", node->type_string());
    else if (node_name == "VarY")
      ASSERT_NE("NGraphVariable", node->type_string());
    else if (node_name == "AssignX")
      ASSERT_EQ("NGraphAssign", node->type_string());
    else if (node_name == "AssignY")
      ASSERT_NE("NGraphAssign", node->type_string());
  }
}

// Test that an Assign with attribute variable_scope = false
// and also the Variable is shared with another Assign so none
// of them captured
TEST(CaptureVariables, SingleVariable) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto init_value = ops::Const(root, {{2.f, 3.f}, {4.f, 5.f}});

  auto var = ops::Variable(root.WithOpName("Var"), varShape, DT_FLOAT);
  auto attr_validate_shape = ops::Assign::Attrs().ValidateShape(false);
  auto var_assign1 = ops::Assign(root.WithOpName("Assign1"), var, init_value,
                                 attr_validate_shape);

  auto var_assign2 = ops::Assign(root.WithOpName("Assign2"), var, init_value);

  std::set<string> skip_these_nodes = {};

  Graph graph(OpRegistry::Global());
  TF_CHECK_OK(root.ToGraph(&graph));

  ASSERT_OK(CaptureVariables(&graph, skip_these_nodes));

  for (auto node : graph.op_nodes()) {
    auto node_name = node->name();
    if (node_name == "Var")
      ASSERT_NE("NGraphVariable", node->type_string());
    else if (node_name == "Assign1")
      ASSERT_NE("NGraphAssign", node->type_string());
    else if (node_name == "Assign2")
      ASSERT_NE("NGraphAssign", node->type_string());
  }
}

}  // namespace testing

}  // namespace ngraph_bridge

}  // namespace tensorflow
