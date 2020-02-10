/*******************************************************************************
 * Copyright 2019-2020 Intel Corporation
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

#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/public/session.h"

#include "gtest/gtest.h"
#include "logging/tf_graph_writer.h"
#include "ngraph_bridge/ngraph_assign_clusters.h"
#include "ngraph_bridge/ngraph_backend_manager.h"
#include "ngraph_bridge/ngraph_mark_for_clustering.h"
#include "test/test_utilities.h"

using namespace std;
namespace ng = ngraph;

namespace tensorflow {

namespace ngraph_bridge {

namespace testing {

// Simple Graph
TEST(VariableTest, SmallGraph1) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto var_assign = ops::Assign(root.WithOpName("Var_Assign"), var, init_value);

  auto c = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto add = ops::Add(root.WithOpName("Add"), var, c);

  auto assign = ops::Assign(root.WithOpName("Assign"), var, add);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;

  ASSERT_OK(ng_session.Run(
      {
          var_assign,
      },
      &ng_outputs1));
  std::vector<string> ng_outputs1_s = ConvertToString(ng_outputs1);

  for (int i = 0; i < 20; i++) {
    ASSERT_OK(ng_session.Run({assign}, &ng_outputs2));
  }

  ASSERT_OK(ng_session.Run({var}, &ng_outputs3));

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;

  ASSERT_OK(tf_session.Run(
      {
          var_assign,
      },
      &tf_outputs1));
  std::vector<string> tf_outputs1_s = ConvertToString(tf_outputs1);

  for (int i = 0; i < 20; i++) {
    ASSERT_OK(tf_session.Run({assign}, &tf_outputs2));
  }

  ASSERT_OK(tf_session.Run({var}, &tf_outputs3));

  Compare(tf_outputs1_s, ng_outputs1_s);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);

  // For other test cases
  ActivateNGraph();
}

// Graph with AssignAdd and AssignSub
TEST(VariableTest, SmallGraph2) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var1"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{2.f, 3.f}, {4.f, 5.f}});
  auto var_assign = ops::Assign(root, var, init_value);

  auto c = ops::Const(root, {{11.f, 12.f}, {13.f, 14.f}});

  auto add = ops::Add(root.WithOpName("Add1"), var, c);

  auto assign_add = ops::AssignAdd(root.WithOpName("AssignAdd"), var, add);

  auto add2 = ops::Add(root.WithOpName("Add2"), assign_add, c);

  auto assign_sub = ops::AssignSub(root.WithOpName("AssignSub"), var, add2);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;

  ASSERT_OK(ng_session.Run({var_assign}, &ng_outputs1));
  std::vector<string> ng_outputs1_s = ConvertToString(ng_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign_sub}, &ng_outputs2));
  }

  ASSERT_OK(ng_session.Run({var}, &ng_outputs3));

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;

  ASSERT_OK(tf_session.Run(
      {
          var_assign,
      },
      &tf_outputs1));
  std::vector<string> tf_outputs1_s = ConvertToString(tf_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign_sub}, &tf_outputs2));
  }

  ASSERT_OK(tf_session.Run({var}, &tf_outputs3));

  Compare(tf_outputs1_s, ng_outputs1_s);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);

  ActivateNGraph();
}

// Graph withApplyGradientDescent
TEST(VariableTest, SmallGraph3) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var1"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto var_assign = ops::Assign(root.WithOpName("Assign1"), var, init_value);

  auto c = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto s = ops::Const(root, 1.f);
  auto d = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto add = ops::Add(root, var, c);

  auto assign_sub = ops::AssignSub(root.WithOpName("AssignSub"), var, add);

  auto apply_gradient_descent =
      ops::ApplyGradientDescent(root.WithOpName("AGD"), assign_sub, s, d);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;
  std::vector<tensorflow::Tensor> ng_outputs4;
  std::vector<tensorflow::Tensor> ng_outputs5;

  ASSERT_OK(ng_session.Run(
      {
          var_assign,
      },
      &ng_outputs1));
  std::vector<string> ng_outputs1_s = ConvertToString(ng_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign_sub}, &ng_outputs2));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({apply_gradient_descent}, &ng_outputs3));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign_sub}, &ng_outputs4));
  }

  ASSERT_OK(ng_session.Run({var}, &ng_outputs5));

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;
  std::vector<tensorflow::Tensor> tf_outputs4;
  std::vector<tensorflow::Tensor> tf_outputs5;

  ASSERT_OK(tf_session.Run(
      {
          var_assign,
      },
      &tf_outputs1));
  std::vector<string> tf_outputs1_s = ConvertToString(tf_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign_sub}, &tf_outputs2));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({apply_gradient_descent}, &tf_outputs3));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign_sub}, &tf_outputs4));
  }

  ASSERT_OK(tf_session.Run({var}, &tf_outputs5));
  Compare(tf_outputs1_s, ng_outputs1_s);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);
  Compare(tf_outputs4, ng_outputs4);
  Compare(tf_outputs5, ng_outputs5);

  ActivateNGraph();
}

// Graph with 2 Variables
TEST(VariableTest, SmallGraph4) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var1 = ops::Variable(root.WithOpName("Var1"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto var1_assign =
      ops::Assign(root.WithOpName("Var1_Assign"), var1, init_value);

  auto var2 = ops::Variable(root.WithOpName("Var2"), varShape, DT_FLOAT);
  auto init_value2 = ops::Const(root, {{123.f, 34.f}, {0.f, 112121.f}});
  auto var2_assign =
      ops::Assign(root.WithOpName("Var2_Assign"), var2, init_value2);

  auto s = ops::Const(root, 1.f);
  auto d = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto add = ops::Add(root.WithOpName("Add"), var1, var2);
  auto assign = ops::Assign(root.WithOpName("Assign"), var1, add);
  auto apply_gradient_descent =
      ops::ApplyGradientDescent(root.WithOpName("AGD"), var2, s, d);
  auto mul = ops::Mul(root.WithOpName("Mul"), var1, var2);
  auto assign2 = ops::Assign(root.WithOpName("Assign2"), var2, mul);
  auto mul2 = ops::Mul(root.WithOpName("Mul2"), var1, var2);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;
  std::vector<tensorflow::Tensor> ng_outputs4;
  std::vector<tensorflow::Tensor> ng_outputs5;

  ASSERT_OK(ng_session.Run({var1_assign, var2_assign}, &ng_outputs1));
  std::vector<string> ng_outputs1_s = ConvertToString(ng_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign}, &ng_outputs2));
  }

  for (int i = 0; i < 5; i++) {
    ASSERT_OK(ng_session.Run({apply_gradient_descent}, &ng_outputs3));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({mul2}, &ng_outputs4));
  }

  ASSERT_OK(ng_session.Run({var1, var2}, &ng_outputs5));

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;
  std::vector<tensorflow::Tensor> tf_outputs4;
  std::vector<tensorflow::Tensor> tf_outputs5;

  ASSERT_OK(tf_session.Run({var1_assign, var2_assign}, &tf_outputs1));
  std::vector<string> tf_outputs1_s = ConvertToString(tf_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign}, &tf_outputs2));
  }

  for (int i = 0; i < 5; i++) {
    ASSERT_OK(tf_session.Run({apply_gradient_descent}, &tf_outputs3));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({mul2}, &tf_outputs4));
  }

  ASSERT_OK(tf_session.Run({var1, var2}, &tf_outputs5));

  Compare(tf_outputs1_s, ng_outputs1_s);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);
  Compare(tf_outputs4, ng_outputs4);
  Compare(tf_outputs5, ng_outputs5);
  ActivateNGraph();
}

// Simple Graph with one Assign Op and it's attribute variable_shape = false
TEST(VariableTest, SmallGraph5) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto attr_validate_shape = ops::Assign::Attrs().ValidateShape(false);
  auto var_assign = ops::Assign(root.WithOpName("Var_Assign"), var, init_value,
                                attr_validate_shape);

  auto c = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto add = ops::Add(root.WithOpName("Add"), var, c);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;

  ASSERT_OK(ng_session.Run(
      {
          var_assign,
      },
      &ng_outputs1));
  std::vector<string> ng_outputs1_s = ConvertToString(ng_outputs1);

  for (int i = 0; i < 20; i++) {
    ASSERT_OK(ng_session.Run({add}, &ng_outputs2));
  }

  ASSERT_OK(ng_session.Run({var}, &ng_outputs3));

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;

  ASSERT_OK(tf_session.Run(
      {
          var_assign,
      },
      &tf_outputs1));
  std::vector<string> tf_outputs1_s = ConvertToString(tf_outputs1);

  for (int i = 0; i < 20; i++) {
    ASSERT_OK(tf_session.Run({add}, &tf_outputs2));
  }

  ASSERT_OK(tf_session.Run({var}, &tf_outputs3));

  Compare(tf_outputs1_s, ng_outputs1_s);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);

  // For other test cases
  ActivateNGraph();
}

// Simple Graph with two Assign Op's and one has attribute
// variable_shape = false while the other has it set to true
TEST(VariableTest, SmallGraph6) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto attr_validate_shape = ops::Assign::Attrs().ValidateShape(false);
  auto var_assign = ops::Assign(root.WithOpName("Var_Assign"), var, init_value,
                                attr_validate_shape);

  auto c = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto add = ops::Add(root.WithOpName("Add"), var, c);

  auto assign = ops::Assign(root.WithOpName("Assign"), var, add);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;

  ASSERT_OK(ng_session.Run(
      {
          var_assign,
      },
      &ng_outputs1));
  std::vector<string> ng_outputs1_s = ConvertToString(ng_outputs1);

  for (int i = 0; i < 20; i++) {
    ASSERT_OK(ng_session.Run({assign}, &ng_outputs2));
  }

  ASSERT_OK(ng_session.Run({var}, &ng_outputs3));

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;

  ASSERT_OK(tf_session.Run(
      {
          var_assign,
      },
      &tf_outputs1));
  std::vector<string> tf_outputs1_s = ConvertToString(tf_outputs1);

  for (int i = 0; i < 20; i++) {
    ASSERT_OK(tf_session.Run({assign}, &tf_outputs2));
  }

  ASSERT_OK(tf_session.Run({var}, &tf_outputs3));

  Compare(tf_outputs1_s, ng_outputs1_s);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);

  // For other test cases
  ActivateNGraph();
}

// Simple Graph for testing the NGVarUpdateNGTensorOp
// end to end
// - execution terminates at the TF optimizer
TEST(VariableTest, SmallGraph7) {
  list<string> env_vars{"NGRAPH_TF_NGVARIABLE_BUFFER_SHARING"};
  const unordered_map<string, string>& env_map = StoreEnv(env_vars);
  SetEnvVariable("NGRAPH_TF_NGVARIABLE_BUFFER_SHARING", "0");

  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto var_assign = ops::Assign(root.WithOpName("Assign1"), var, init_value);

  auto accum = ops::Variable(root.WithOpName("accum"), varShape, DT_FLOAT);
  auto init_value2 = ops::Const(root, {{3.f, 3.f}, {3.f, 3.f}});
  auto accum_assign =
      ops::Assign(root.WithOpName("Assign2"), accum, init_value2);

  auto grad = ops::Const(root, {{2.f, 2.f}, {2.f, 2.f}});

  auto lr = ops::Const(root, 1.f);

  ops::ApplyAdagrad::Attrs use_locking;
  use_locking = use_locking.UseLocking(true);
  auto applyadagrad_t = ops::ApplyAdagrad(root.WithOpName("Adagrad"), var,
                                          accum, lr, grad, use_locking);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;

  ASSERT_OK(ng_session.Run({{var_assign, accum_assign}}, &ng_outputs1));
  std::vector<string> ng_outputs1_s = ConvertToString(ng_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({applyadagrad_t}, &ng_outputs2));
  }
  DeactivateNGraph();

  // Run on TF
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;

  ASSERT_OK(tf_session.Run({{var_assign, accum_assign}}, &tf_outputs1));
  std::vector<string> tf_outputs1_s = ConvertToString(tf_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({applyadagrad_t}, &tf_outputs2));
  }

  Compare(ng_outputs1_s, tf_outputs1_s);
  Compare(tf_outputs2, ng_outputs2);

  ActivateNGraph();
  UnsetEnvVariable("NGRAPH_TF_NGVARIABLE_BUFFER_SHARING");
  RestoreEnv(env_map);
}  // SmallGraph7

// - output of TF optimizer is fed to another op
// e.g. Add op that is supported
TEST(VariableTest, SmallGraph8) {
  list<string> env_vars{"NGRAPH_TF_NGVARIABLE_BUFFER_SHARING"};
  const unordered_map<string, string>& env_map = StoreEnv(env_vars);
  SetEnvVariable("NGRAPH_TF_NGVARIABLE_BUFFER_SHARING", "0");

  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto var_assign = ops::Assign(root.WithOpName("Assign1"), var, init_value);

  auto accum = ops::Variable(root.WithOpName("accum"), varShape, DT_FLOAT);
  auto init_value2 = ops::Const(root, {{3.f, 3.f}, {3.f, 3.f}});
  auto accum_assign =
      ops::Assign(root.WithOpName("Assign2"), accum, init_value2);

  auto grad = ops::Const(root, {{2.f, 2.f}, {2.f, 2.f}});

  auto lr = ops::Const(root, 1.f);

  ops::ApplyAdagrad::Attrs use_locking;
  use_locking = use_locking.UseLocking(true);
  auto applyadagrad_t = ops::ApplyAdagrad(root.WithOpName("Adagrad"), var,
                                          accum, lr, grad, use_locking);

  auto c = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto add = ops::Add(root.WithOpName("Add"), applyadagrad_t, c);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;

  ASSERT_OK(ng_session.Run({{var_assign, accum_assign}}, &ng_outputs1));
  std::vector<string> ng_outputs1_s = ConvertToString(ng_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({applyadagrad_t}, &ng_outputs2));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({add}, &ng_outputs3));
  }

  DeactivateNGraph();

  // Run on TF
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;

  ASSERT_OK(tf_session.Run({{var_assign, accum_assign}}, &tf_outputs1));
  std::vector<string> tf_outputs1_s = ConvertToString(tf_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({applyadagrad_t}, &tf_outputs2));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({add}, &tf_outputs3));
  }

  Compare(tf_outputs1_s, ng_outputs1_s);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);

  ActivateNGraph();
  UnsetEnvVariable("NGRAPH_TF_NGVARIABLE_BUFFER_SHARING");
  RestoreEnv(env_map);
}  // SmallGraph8

// - output of TF optimizer is fed to a variable modifier
// that is supported e.g. AssignAdd
TEST(VariableTest, SmallGraph9) {
  list<string> env_vars{"NGRAPH_TF_NGVARIABLE_BUFFER_SHARING"};
  const unordered_map<string, string>& env_map = StoreEnv(env_vars);
  SetEnvVariable("NGRAPH_TF_NGVARIABLE_BUFFER_SHARING", "0");

  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto var_assign = ops::Assign(root.WithOpName("Assign1"), var, init_value);

  auto accum = ops::Variable(root.WithOpName("accum"), varShape, DT_FLOAT);
  auto init_value2 = ops::Const(root, {{3.f, 3.f}, {3.f, 3.f}});
  auto accum_assign =
      ops::Assign(root.WithOpName("Assign2"), accum, init_value2);

  auto grad = ops::Const(root, {{2.f, 2.f}, {2.f, 2.f}});

  auto lr = ops::Const(root, 1.f);

  ops::ApplyAdagrad::Attrs use_locking;
  use_locking = use_locking.UseLocking(true);
  auto applyadagrad_t = ops::ApplyAdagrad(root.WithOpName("Adagrad"), var,
                                          accum, lr, grad, use_locking);

  auto var1 = ops::Variable(root.WithOpName("Var1"), varShape, DT_FLOAT);
  auto init_value1 = ops::Const(root, {{2.f, 3.f}, {4.f, 5.f}});
  auto var1_assign = ops::Assign(root, var1, init_value1);

  auto assign_add =
      ops::AssignAdd(root.WithOpName("AssignAdd"), var1, applyadagrad_t);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;
  std::vector<tensorflow::Tensor> ng_outputs4;

  ASSERT_OK(
      ng_session.Run({{var_assign, accum_assign, var1_assign}}, &ng_outputs1));
  std::vector<string> ng_outputs1_s = ConvertToString(ng_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({applyadagrad_t}, &ng_outputs2));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign_add}, &ng_outputs3));
  }
  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({var1}, &ng_outputs4));
  }

  DeactivateNGraph();

  // Run on TF
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;
  std::vector<tensorflow::Tensor> tf_outputs4;

  ASSERT_OK(
      tf_session.Run({{var_assign, accum_assign, var1_assign}}, &tf_outputs1));
  std::vector<string> tf_outputs1_s = ConvertToString(tf_outputs1);

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({applyadagrad_t}, &tf_outputs2));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign_add}, &tf_outputs3));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({var1}, &tf_outputs4));
  }

  Compare(tf_outputs1_s, ng_outputs1_s);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);
  Compare(tf_outputs4, ng_outputs4);

  ActivateNGraph();
  UnsetEnvVariable("NGRAPH_TF_NGVARIABLE_BUFFER_SHARING");
  RestoreEnv(env_map);
}  // SmallGraph9

}  // namespace testing
}  // namespace ngraph_bridge
}  // namespace tensorflow
