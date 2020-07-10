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
#include <map>

#include "gtest/gtest.h"

#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/common_runtime/dma_helper.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_types.h"
#include "tensorflow/core/graph/algorithm.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/graph/graph_constructor.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/public/session.h"

#include <cmath>
#include "logging/tf_graph_writer.h"
#include "ngraph_bridge/ngraph_utils.h"
#include "test/opexecuter.h"
#include "test/test_utilities.h"

using namespace std;
namespace ng = ngraph;

namespace tensorflow {

namespace ngraph_bridge {

namespace testing {

// Test(TestCaseName, TestName)
// Please ensure
// Neither TestCaseName nor TestName should contain underscore
// https://github.com/google/googletest/blob/master/googletest/docs/primer.md
// Use only Tensors and ops::Const() to provide input to the test op
// Please ensure the alphabetical order while adding the test functions

// Test op: Abs
TEST(MathOps, Abs1D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 1;
  Tensor A(DT_FLOAT, TensorShape({dim1}));

  AssignInputValuesRandom(A);

  vector<int> static_input_indexes = {};
  auto R = ops::Abs(root, A);
  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Abs", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();

  auto ng_function = opexecuter.get_ng_function();
  auto node_list = ng_function->get_ordered_ops();
  // Since its a unary op, get_ordered_op will produce a total ordering, and
  // hence we can be sure the first is the arg, and the second is the op, and
  // the third is the retval. In multiple test runs the retval's number changes,
  // hence not adding in an assert
  ASSERT_EQ(node_list.size(), 3);
  auto it = node_list.begin();
  ASSERT_EQ((*std::next(it))->get_friendly_name(), "Abs");
}

TEST(MathOps, Abs2D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 4;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValuesRandom(A);

  vector<int> static_input_indexes = {};
  auto R = ops::Abs(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Abs", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Abs

// Test op: Acos
TEST(MathOps, Acos2D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 4;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValuesRandom(A);

  vector<int> static_input_indexes = {};
  auto R = ops::Acos(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Acos", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Acos

// Test op: Add
TEST(MathOps, Add) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 2.1f);
  AssignInputValues(B, 4.1f);

  vector<int> static_input_indexes = {};
  auto R = ops::Add(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Add", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Add

// Test op: AddV2
TEST(MathOps, AddV2) {
  // Run a bunch of sub-test combinations to check shape broadcasting
  vector<TensorShape> tensors_combs = {
      // A-size, B-size
      {2, 4}, {2, 4},  // sub-test# 1
      {2, 4}, {2, 1},  // sub-test# 2
      {2, 4}, {1, 4},  // sub-test# 3
      {2, 1}, {2, 4},  // sub-test# 4
      {1, 4}, {2, 4},  // sub-test# 5
      {2, 4}, {1, 1},  // sub-test# 6
      {1, 1}, {2, 4},  // sub-test# 7
  };

  for (int i = 0; i < tensors_combs.size(); i += 2) {
    NGRAPH_VLOG(5) << "========>> Running AddV2 sub-test# " << (int)(i / 2 + 1)
                   << " ...";

    Scope root = Scope::NewRootScope();

    Tensor A(DT_FLOAT, TensorShape(tensors_combs[i]));
    Tensor B(DT_FLOAT, TensorShape(tensors_combs[i + 1]));

    AssignInputValues(A, 2.1f);
    AssignInputValues(B, 4.1f);

    vector<int> static_input_indexes = {};
    auto R = ops::AddV2(root, A, B);

    vector<DataType> output_datatypes = {DT_FLOAT};
    std::vector<Output> sess_run_fetchoutputs = {R};
    OpExecuter opexecuter(root, "AddV2", static_input_indexes, output_datatypes,
                          sess_run_fetchoutputs);

    opexecuter.RunTest();
  }

}  // end of test op AddV2

// Test op: AddN
TEST(MathOps, AddN) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor C(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 4.5f);
  AssignInputValues(B, 3.2f);
  AssignInputValues(C, 2.3f);

  vector<int> static_input_indexes = {};
  auto R = ops::AddN(root, {A, B, C});

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "AddN", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op AddN

// Test op: Any
// Any with attribute KeepDims set to true
// Fails with opset3 upgrade because there is no opset0
// downgrade available for it in nGraph
TEST(MathOps, DISABLED_AnyKeepDims) {
  int dim1 = 2;
  int dim2 = 2;
  std::vector<bool> v = {true, true, true, true};

  Tensor A(DT_BOOL, TensorShape({dim1, dim2}));
  auto keep_dims = ops::Any::Attrs().KeepDims(true);
  AssignInputValues<bool>(A, v);
  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  int axis = 0;
  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_BOOL};

  Scope root = Scope::NewRootScope();
  auto R = ops::Any(root, A, axis, keep_dims);
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Any", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}

TEST(MathOps, DISABLED_AnyNegativeAxis) {
  int dim1 = 2;
  int dim2 = 3;
  std::vector<bool> v = {true, true, true, true, false, false};

  Tensor A(DT_BOOL, TensorShape({dim1, dim2}));
  AssignInputValues<bool>(A, v);
  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  int axis = -1;
  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_BOOL};

  Scope root = Scope::NewRootScope();
  auto R = ops::Any(root, A, axis);
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Any", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}

TEST(MathOps, DISABLED_AnyPositiveAxis) {
  int dim1 = 3;
  int dim2 = 3;
  std::vector<bool> v = {true,  true, true,  true, false,
                         false, true, false, false};

  Tensor A(DT_BOOL, TensorShape({dim1, dim2}));
  AssignInputValues<bool>(A, v);
  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  int axis = 1;
  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_BOOL};

  Scope root = Scope::NewRootScope();
  auto R = ops::Any(root, A, axis);
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Any", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Any

// Test op: All
// All with attribute KeepDims set to true
// Fails with opset3 upgrade because there is no opset0
// downgrade available for it in nGraph
TEST(MathOps, DISABLED_AllKeepDims) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  std::vector<bool> v = {true, true, true, false};
  Tensor A(DT_BOOL, TensorShape({dim1, dim2}));
  auto keep_dims = ops::All::Attrs().KeepDims(true);

  AssignInputValues<bool>(A, v);

  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  int axis = 0;

  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_BOOL};

  auto R = ops::All(root, A, axis, keep_dims);
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "All", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}

TEST(MathOps, DISABLED_AllNegativeAxis) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 3;

  std::vector<bool> v = {true, true, true, true, false, false};
  Tensor A(DT_BOOL, TensorShape({dim1, dim2}));

  AssignInputValues<bool>(A, v);

  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  int axis = -1;

  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_BOOL};

  auto R = ops::All(root, A, axis);
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "All", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}

TEST(MathOps, DISABLED_AllPositiveAxis) {
  Scope root = Scope::NewRootScope();
  int dim1 = 3;
  int dim2 = 3;

  std::vector<bool> v = {true,  true, true,  true, false,
                         false, true, false, false};
  Tensor A(DT_BOOL, TensorShape({dim1, dim2}));

  AssignInputValues<bool>(A, v);

  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  int axis = 1;

  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_BOOL};

  auto R = ops::All(root, A, axis);
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "All", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op All

// Test op: Asin
TEST(MathOps, Asin) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 4;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValuesRandom(A);

  vector<int> static_input_indexes = {};
  auto R = ops::Asin(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Asin", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Asin

// Test op: Atan
TEST(MathOps, Atan) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1}));

  AssignInputValuesRandom(A);

  vector<int> static_input_indexes = {};
  auto R = ops::Atan(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Atan", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Atan

// Test op: Cumsum
TEST(MathOps, Cumsum) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_INT32, TensorShape({}));

  AssignInputValues(A, 2.1f);
  AssignInputValues(B, 0);

  vector<int> static_input_indexes = {};
  auto attrs = ops::Cumsum::Attrs();
  attrs.exclusive_ = true;
  attrs.reverse_ = true;
  auto R = ops::Cumsum(root, A, B, attrs);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Cumsum", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Cumsum

// Test op: Sum with & without keep dims & with both positive & negative axis
TEST(MathOps, Sum) {
  int dim1 = 2;
  int dim2 = 2;

  std::vector<int> v = {1, 2, 3, 4};
  Tensor A(DT_INT32, TensorShape({dim1, dim2}));
  vector<bool> v_keep_dims = {true, false};
  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  vector<int> v_axis = {-1, 0, 1};
  for (auto axis : v_axis) {
    for (auto keep_dims : v_keep_dims) {
      Scope root = Scope::NewRootScope();
      auto keep_dims_attr = ops::Sum::Attrs().KeepDims(keep_dims);

      AssignInputValues<int>(A, v);

      vector<int> static_input_indexes = {1};
      vector<DataType> output_datatypes = {DT_INT32};

      auto R = ops::Sum(root, A, axis, keep_dims_attr);
      std::vector<Output> sess_run_fetchoutputs = {R};
      OpExecuter opexecuter(root, "Sum", static_input_indexes, output_datatypes,
                            sess_run_fetchoutputs);

      opexecuter.RunTest();
    }
  }
}

// Test op: Mean with & without keep dims & with both positive & negative axis
TEST(MathOps, Mean) {
  int dim1 = 2;
  int dim2 = 2;

  std::vector<int> v = {1, 2, 3, 4};
  Tensor A(DT_INT32, TensorShape({dim1, dim2}));
  vector<bool> v_keep_dims = {true, false};
  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  vector<int> v_axis = {-1, 0, 1};
  for (auto axis : v_axis) {
    for (auto keep_dims : v_keep_dims) {
      Scope root = Scope::NewRootScope();
      auto keep_dims_attr = ops::Mean::Attrs().KeepDims(keep_dims);

      AssignInputValues<int>(A, v);

      vector<int> static_input_indexes = {1};
      vector<DataType> output_datatypes = {DT_INT32};

      auto R = ops::Mean(root, A, axis, keep_dims_attr);
      std::vector<Output> sess_run_fetchoutputs = {R};
      OpExecuter opexecuter(root, "Mean", static_input_indexes,
                            output_datatypes, sess_run_fetchoutputs);

      opexecuter.RunTest();
    }
  }
}

// Test op: Prod with & without keep dims & with both positive & negative axis
TEST(MathOps, Prod) {
  int dim1 = 2;
  int dim2 = 2;

  std::vector<int> v = {1, 2, 3, 4};
  Tensor A(DT_INT32, TensorShape({dim1, dim2}));
  vector<bool> v_keep_dims = {true, false};
  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  vector<int> v_axis = {-1, 0, 1};
  for (auto axis : v_axis) {
    for (auto keep_dims : v_keep_dims) {
      Scope root = Scope::NewRootScope();
      auto keep_dims_attr = ops::Prod::Attrs().KeepDims(keep_dims);

      AssignInputValues<int>(A, v);

      vector<int> static_input_indexes = {1};
      vector<DataType> output_datatypes = {DT_INT32};

      auto R = ops::Prod(root, A, axis, keep_dims_attr);
      std::vector<Output> sess_run_fetchoutputs = {R};
      OpExecuter opexecuter(root, "Prod", static_input_indexes,
                            output_datatypes, sess_run_fetchoutputs);

      opexecuter.RunTest();
    }
  }
}

// ArgMax test for negative dimension
TEST(MathOps, ArgMaxNeg) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValuesRandom(A);

  int dim = -1;

  vector<int> static_input_indexes = {1};

  auto R = ops::ArgMax(root, A, dim);

  vector<DataType> output_datatypes = {DT_INT64};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "ArgMax", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}

// ArgMax test for positive dimension
TEST(MathOps, ArgMaxPos) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValuesRandom(A);

  int dim = 1;

  vector<int> static_input_indexes = {1};

  auto attrs = ops::ArgMax::Attrs();
  attrs.output_type_ = DT_INT32;

  auto R = ops::ArgMax(root, A, dim, attrs);

  vector<DataType> output_datatypes = {DT_INT32};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "ArgMax", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op ArgMax

// ArgMin test for negative dimension
TEST(MathOps, ArgMinNeg) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValuesRandom(A);

  int dim = -1;

  vector<int> static_input_indexes = {1};

  auto R = ops::ArgMin(root, A, dim);

  vector<DataType> output_datatypes = {DT_INT64};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "ArgMin", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}

// ArgMin test for positive dimension
TEST(MathOps, ArgMinPos) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValuesRandom(A);

  int dim = 1;

  vector<int> static_input_indexes = {1};

  auto attrs = ops::ArgMin::Attrs();
  attrs.output_type_ = DT_INT32;

  auto R = ops::ArgMin(root, A, dim, attrs);

  vector<DataType> output_datatypes = {DT_INT32};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "ArgMin", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op ArgMin

// Test op: Atan2
TEST(MathOps, Atan2) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 5;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues<float>(A, {0, -0, 3, -3.5, 1.2, 3, 5, -4.5, 1.0, -7.0});
  AssignInputValues<float>(B, {0, -0, 3, 2.5, -0.7, 2, 3.4, -5.6, 30, 0.06});

  vector<int> static_input_indexes = {};
  auto R = ops::Atan2(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Atan2", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Atan2

// Test op: MatMul
TEST(MathOps, MatMul) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3}));
  Tensor B(DT_FLOAT, TensorShape({3, 4}));

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::MatMul(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "MatMul", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// Test op: BatchMatMul
// BatchMatMul2D
// AdjX = false
// AdjY = false
TEST(MathOps, BatchMatMul2D) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3}));
  Tensor B(DT_FLOAT, TensorShape({3, 4}));

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::BatchMatMul(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul2D
// AdjX = true
// AdjY = false
TEST(MathOps, BatchMatMul2DAdjX) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3}));
  Tensor B(DT_FLOAT, TensorShape({2, 4}));

  auto attrs_x = ops::BatchMatMul::Attrs();
  attrs_x = attrs_x.AdjX(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMul(root, A, B, attrs_x);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul2D
// AdjX = false
// AdjY = true
TEST(MathOps, BatchMatMul2DAdjY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 4}));
  Tensor B(DT_FLOAT, TensorShape({3, 4}));

  auto attrs_y = ops::BatchMatMul::Attrs();
  attrs_y = attrs_y.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::BatchMatMul(root, A, B, attrs_y);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul2D
// AdjX = true
// AdjY = true
TEST(MathOps, BatchMatMul2DAdjXY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 4}));
  Tensor B(DT_FLOAT, TensorShape({3, 2}));

  auto attrs_xy = ops::BatchMatMul::Attrs();
  attrs_xy = attrs_xy.AdjY(true);
  attrs_xy = attrs_xy.AdjX(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::BatchMatMul(root, A, B, attrs_xy);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul3D
// AdjX = false
// AdjY = false
TEST(MathOps, BatchMatMul3D) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4}));
  Tensor B(DT_FLOAT, TensorShape({2, 4, 5}));

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMul(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul3D
// AdjX = true
// AdjY = false
TEST(MathOps, BatchMatMul3DAdjX) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 5}));

  auto attrs_x = ops::BatchMatMul::Attrs();
  attrs_x = attrs_x.AdjX(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMul(root, A, B, attrs_x);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul3D
// AdjX = false
// AdjY = true
TEST(MathOps, BatchMatMul3DAdjY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 4, 3}));
  Tensor B(DT_FLOAT, TensorShape({2, 5, 3}));

  auto attrs_y = ops::BatchMatMul::Attrs();
  attrs_y = attrs_y.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMul(root, A, B, attrs_y);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul 3D
// AdjX = true
// AdjY = true
TEST(MathOps, BatchMatMul3DAdjXY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 4}));

  auto attrs_xy = ops::BatchMatMul::Attrs();
  attrs_xy = attrs_xy.AdjX(true);
  attrs_xy = attrs_xy.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMul(root, A, B, attrs_xy);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul 4D
// AdjX = false
// AdjY = false
TEST(MathOps, BatchMatMul4D) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 5, 1}));

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMul(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul 4D
// AdjX = true
// AdjY = false
TEST(MathOps, BatchMatMul4DAdjX) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 4, 1}));

  auto attrs_x = ops::BatchMatMul::Attrs();
  attrs_x = attrs_x.AdjX(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMul(root, A, B, attrs_x);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul 4D
// AdjX = false
// AdjY = true
TEST(MathOps, BatchMatMul4DAdjY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 5, 5}));

  auto attrs_y = ops::BatchMatMul::Attrs();
  attrs_y = attrs_y.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMul(root, A, B, attrs_y);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMul 4D
// AdjX = true
// AdjY = true
TEST(MathOps, BatchMatMul4DAdjXY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 1, 4}));

  auto attrs_xy = ops::BatchMatMul::Attrs();
  attrs_xy = attrs_xy.AdjX(true);
  attrs_xy = attrs_xy.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMul(root, A, B, attrs_xy);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMul", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op BatchMatMul

// Test op: BatchMatMulV2
// BatchMatMulV22D
// AdjX = false
// AdjY = false
TEST(MathOps, BatchMatMulV22D) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3}));
  Tensor B(DT_FLOAT, TensorShape({3, 4}));

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::BatchMatMulV2(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV22D
// AdjX = true
// AdjY = false
TEST(MathOps, BatchMatMulV22DAdjX) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3}));
  Tensor B(DT_FLOAT, TensorShape({2, 4}));

  auto attrs_x = ops::BatchMatMulV2::Attrs();
  attrs_x = attrs_x.AdjX(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMulV2(root, A, B, attrs_x);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV22D
// AdjX = false
// AdjY = true
TEST(MathOps, BatchMatMulV22DAdjY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 4}));
  Tensor B(DT_FLOAT, TensorShape({3, 4}));

  auto attrs_y = ops::BatchMatMulV2::Attrs();
  attrs_y = attrs_y.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::BatchMatMulV2(root, A, B, attrs_y);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV22D
// AdjX = true
// AdjY = true
TEST(MathOps, BatchMatMulV22DAdjXY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 4}));
  Tensor B(DT_FLOAT, TensorShape({3, 2}));

  auto attrs_xy = ops::BatchMatMulV2::Attrs();
  attrs_xy = attrs_xy.AdjY(true);
  attrs_xy = attrs_xy.AdjX(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::BatchMatMulV2(root, A, B, attrs_xy);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV23D
// AdjX = false
// AdjY = false
TEST(MathOps, BatchMatMulV23D) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4}));
  Tensor B(DT_FLOAT, TensorShape({2, 4, 5}));

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMulV2(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV23D
// AdjX = true
// AdjY = false
TEST(MathOps, BatchMatMulV23DAdjX) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 5}));

  auto attrs_x = ops::BatchMatMulV2::Attrs();
  attrs_x = attrs_x.AdjX(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMulV2(root, A, B, attrs_x);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV23D
// AdjX = false
// AdjY = true
TEST(MathOps, BatchMatMulV23DAdjY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 4, 3}));
  Tensor B(DT_FLOAT, TensorShape({2, 5, 3}));

  auto attrs_y = ops::BatchMatMulV2::Attrs();
  attrs_y = attrs_y.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMulV2(root, A, B, attrs_y);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV2 3D
// AdjX = true
// AdjY = true
TEST(MathOps, BatchMatMulV23DAdjXY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 4}));

  auto attrs_xy = ops::BatchMatMulV2::Attrs();
  attrs_xy = attrs_xy.AdjX(true);
  attrs_xy = attrs_xy.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMulV2(root, A, B, attrs_xy);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV2 4D
// AdjX = false
// AdjY = false
TEST(MathOps, BatchMatMulV24D) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 5, 1}));

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMulV2(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV2 4D
// AdjX = true
// AdjY = false
TEST(MathOps, BatchMatMulV24DAdjX) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 4, 1}));

  auto attrs_x = ops::BatchMatMulV2::Attrs();
  attrs_x = attrs_x.AdjX(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMulV2(root, A, B, attrs_x);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV2 4D
// AdjX = false
// AdjY = true
TEST(MathOps, BatchMatMulV24DAdjY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 5, 5}));

  auto attrs_y = ops::BatchMatMulV2::Attrs();
  attrs_y = attrs_y.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMulV2(root, A, B, attrs_y);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}

// BatchMatMulV2 4D
// AdjX = true
// AdjY = true
TEST(MathOps, BatchMatMulV24DAdjXY) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({2, 3, 4, 5}));
  Tensor B(DT_FLOAT, TensorShape({2, 3, 1, 4}));

  auto attrs_xy = ops::BatchMatMulV2::Attrs();
  attrs_xy = attrs_xy.AdjX(true);
  attrs_xy = attrs_xy.AdjY(true);

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};

  auto R = ops::BatchMatMulV2(root, A, B, attrs_xy);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "BatchMatMulV2", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op BatchMatMulV2

// Test op: Cast : float to int
TEST(MathOps, Cast1D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1}));

  AssignInputValuesRandom(A);

  vector<int> static_input_indexes = {};
  auto R = ops::Cast(root, A, DT_INT32);

  vector<DataType> output_datatypes = {DT_INT32};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Cast", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}

TEST(MathOps, Cast2D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValuesRandom(A);

  vector<int> static_input_indexes = {};
  auto R = ops::Cast(root, A, DT_INT32);

  vector<DataType> output_datatypes = {DT_INT32};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Cast", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Cast

// Test op: Ceil
TEST(MathOps, Ceil) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 5;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValuesRandom(A);

  vector<int> static_input_indexes = {};
  auto R = ops::Ceil(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Ceil", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Ceil

// Test op: Cos
TEST(MathOps, Cos) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 5;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues<float>(
      A, {0, -0, M_PI / 2, M_PI, 1.0, 3.8, 4.2, -3.9, -4.2, -1.0});

  vector<int> static_input_indexes = {};
  auto R = ops::Cos(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Cos", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Cos

// Test op: Cosh
TEST(MathOps, Cosh) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1}));

  AssignInputValuesRandom(A);

  vector<int> static_input_indexes = {};
  auto R = ops::Cosh(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Cosh", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Cosh

// Test op: Exp
TEST(MathOps, Exp1D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1}));

  AssignInputValues(A, 2.5f);

  vector<int> static_input_indexes = {};
  auto R = ops::Exp(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Exp", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}

TEST(MathOps, Exp2D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 3.6f);

  vector<int> static_input_indexes = {};
  auto R = ops::Exp(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Exp", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Exp

// Test op: FloorDiv
TEST(MathOps, FloorDiv) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 4.5f);
  AssignInputValues(B, 3.2f);

  vector<int> static_input_indexes = {};
  auto R = ops::FloorDiv(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "FloorDiv", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op FloorDiv

TEST(MathOps, FloorDivInt) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_INT32, TensorShape({dim1, dim2}));
  Tensor B(DT_INT32, TensorShape({dim1, dim2}));

  AssignInputValues(A, 4);
  AssignInputValues(B, 3);

  vector<int> static_input_indexes = {};
  auto R = ops::FloorDiv(root, A, B);

  vector<DataType> output_datatypes = {DT_INT32};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "FloorDiv", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op FloorDivInt

// Test op: FloorDivBroadcasting
TEST(MathOps, FloorDivBroadcasting) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1}));

  AssignInputValues(A, 4.5f);
  AssignInputValues(B, 3.2f);

  vector<int> static_input_indexes = {};
  auto R = ops::FloorDiv(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "FloorDiv", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op FloorDivBroadcasting

// Test op: FloorDivNegInt
TEST(MathOps, FloorDivNegInt) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_INT32, TensorShape({1}));
  Tensor B(DT_INT32, TensorShape({1}));

  AssignInputValues(A, -1);
  AssignInputValues(B, 3);

  vector<int> static_input_indexes = {};
  auto R = ops::FloorDiv(root, A, B);

  vector<DataType> output_datatypes = {DT_INT32};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "FloorDiv", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op FloorDivNegInt

// For FloorDiv op, the input and output data type should match
TEST(MathOps, FloorDivNegFloat) {
  Scope root = Scope::NewRootScope();

  Tensor A(DT_FLOAT, TensorShape({1}));
  Tensor B(DT_FLOAT, TensorShape({1}));

  AssignInputValues(A, -1.f);
  AssignInputValues(B, 3.f);

  vector<int> static_input_indexes = {};
  auto R = ops::FloorDiv(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "FloorDiv", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op FloorDivNegFloat

// Test op: FloorMod
TEST(MathOps, DISABLED_FloorMod) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 7.5f);
  AssignInputValues(B, 5.2f);

  vector<int> static_input_indexes = {};
  auto R = ops::FloorMod(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "FloorMod", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op FloorMod

// Test op: FloorModBroadcasting
TEST(MathOps, DISABLED_FloorModBroadcasting) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1}));

  AssignInputValues(A, 7.5f);
  AssignInputValues(B, 5.2f);

  vector<int> static_input_indexes = {};
  auto R = ops::FloorMod(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "FloorMod", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op FloorModBroadcasting

// Test op: FloorModNegInt
// Currently failing with TF produces {2,2}, NG produces {-8,-3}
// Should enable when NGraph fixes the FloorMod
TEST(MathOps, DISABLED_FloorModNegInt) {
  Scope root = Scope::NewRootScope();
  vector<int> nums = {-8, -8};
  vector<int> divs = {10, 5};

  Tensor A(DT_INT32, TensorShape({1, 2}));
  Tensor B(DT_INT32, TensorShape({1, 2}));

  AssignInputValues(A, nums);
  AssignInputValues(B, divs);

  vector<int> static_input_indexes = {};
  auto R = ops::FloorMod(root, A, B);
  vector<DataType> output_datatypes = {DT_INT32};
  std::vector<Output> sess_run_fetchoutputs = {R};

  OpExecuter opexecuter(root, "FloorMod", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op FloorModNegInt

TEST(MathOps, DISABLED_FloorModNegFloat) {
  Scope root = Scope::NewRootScope();

  vector<float> nums = {-8.f, -8.f};
  vector<float> divs = {10.f, 5.f};

  Tensor A(DT_FLOAT, TensorShape({1, 2}));
  Tensor B(DT_FLOAT, TensorShape({1, 2}));

  AssignInputValues(A, nums);
  AssignInputValues(B, divs);

  vector<int> static_input_indexes = {};
  auto R = ops::FloorMod(root, A, B);
  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};

  OpExecuter opexecuter(root, "FloorMod", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op FloorModNegFloat

// Test op: IsFinite
TEST(MathOps, IsFinite) {
  Scope root = Scope::NewRootScope();
  int dim1 = 8;

  Tensor A(DT_FLOAT, TensorShape({dim1}));
  std::vector<float> values{0.f,
                            1.f,
                            2.f,
                            -2.f,
                            std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::signaling_NaN()};
  AssignInputValues(A, values);
  vector<int> static_input_indexes = {};
  auto R = ops::IsFinite(root, A);

  vector<DataType> output_datatypes = {DT_BOOL};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "IsFinite", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op IsFinite

// Test op: Log
TEST(MathOps, Log1D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 4;

  Tensor A(DT_FLOAT, TensorShape({dim1}));

  AssignInputValues(A, 1.4f);

  vector<int> static_input_indexes = {};
  auto R = ops::Log(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Log", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}

TEST(MathOps, Log2D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 3.5f);

  vector<int> static_input_indexes = {};
  auto R = ops::Log(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Log", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Log

TEST(MathOps, Log1p) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 4;
  vector<float> vals = {-2, -1, 0, 0.25, 0.5, 1, 5, 10};

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, vals);

  vector<int> static_input_indexes = {};
  auto R = ops::Log1p(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Log1p", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Log1p

// Test Op:LogicalOr
TEST(MathOps, LogicalOr) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 3;
  std::vector<bool> v1 = {true, true, true, true, false, false};
  std::vector<bool> v2 = {false, true, false, true, false, false};

  Tensor A(DT_BOOL, TensorShape({dim1, dim2}));
  AssignInputValues(A, v1);

  Tensor B(DT_BOOL, TensorShape({dim1, dim2}));
  AssignInputValues(B, v2);

  vector<int> static_input_indexes = {};

  auto R = ops::LogicalOr(root, A, B);

  vector<DataType> output_datatypes = {DT_BOOL};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "LogicalOr", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of LogicalOr

// Test Op:LogicalNot
TEST(MathOps, LogicalNot) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 3;
  std::vector<bool> v1 = {true, true, true, true, false, false};

  Tensor A(DT_BOOL, TensorShape({dim1, dim2}));
  AssignInputValues(A, v1);

  vector<int> static_input_indexes = {};

  auto R = ops::LogicalNot(root, A);

  vector<DataType> output_datatypes = {DT_BOOL};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "LogicalNot", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of LogicalNot

// Test op: Max
TEST(MathOps, MaxNegativeAxis) {
  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValuesRandom(A);

  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  vector<int> axis_ = {-1};

  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_FLOAT};

  for (auto const& axis : axis_) {
    Scope root = Scope::NewRootScope();
    auto R = ops::Max(root, A, axis);
    std::vector<Output> sess_run_fetchoutputs = {R};
    OpExecuter opexecuter(root, "Max", static_input_indexes, output_datatypes,
                          sess_run_fetchoutputs);

    opexecuter.RunTest();
  }
}

TEST(MathOps, MaxPositiveAxis) {
  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValuesRandom(A);

  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  vector<int> axis_ = {0};

  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_FLOAT};

  for (auto const& axis : axis_) {
    Scope root = Scope::NewRootScope();
    auto R = ops::Max(root, A, axis);
    std::vector<Output> sess_run_fetchoutputs = {R};
    OpExecuter opexecuter(root, "Max", static_input_indexes, output_datatypes,
                          sess_run_fetchoutputs);

    opexecuter.RunTest();
  }

}  // end of test op Max

// Test op: Min
TEST(MathOps, MinNegativeAxis) {
  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValuesRandom(A);

  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  vector<int> axis_ = {-1};

  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_FLOAT};

  for (auto const& axis : axis_) {
    Scope root = Scope::NewRootScope();
    auto R = ops::Min(root, A, axis);
    std::vector<Output> sess_run_fetchoutputs = {R};
    OpExecuter opexecuter(root, "Min", static_input_indexes, output_datatypes,
                          sess_run_fetchoutputs);

    opexecuter.RunTest();
  }
}

TEST(MathOps, MinPositiveAxis) {
  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValuesRandom(A);

  // axis at which the dimension will be inserted
  // should be -rank <= axis < rank
  vector<int> axis_ = {0};

  vector<int> static_input_indexes = {1};
  vector<DataType> output_datatypes = {DT_FLOAT};

  for (auto const& axis : axis_) {
    Scope root = Scope::NewRootScope();
    auto R = ops::Min(root, A, axis);
    std::vector<Output> sess_run_fetchoutputs = {R};
    OpExecuter opexecuter(root, "Min", static_input_indexes, output_datatypes,
                          sess_run_fetchoutputs);

    opexecuter.RunTest();
  }

}  // end of test op Min

// Test op: Minimum
TEST(MathOps, Minimum) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValuesRandom(A);
  AssignInputValuesRandom(B);

  vector<int> static_input_indexes = {};
  auto R = ops::Minimum(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Minimum", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Minimum

// Test op: MinimumBroadcasting
TEST(MathOps, MinimumBroadcasting) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1}));

  AssignInputValues(A, 7.5f);
  AssignInputValues(B, 5.2f);

  vector<int> static_input_indexes = {};
  auto R = ops::Minimum(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Minimum", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op MinimumBroadcasting

// Test op: MaximumBroadcasting
TEST(MathOps, MaximumBroadcasting) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1}));

  AssignInputValues(A, 7.5f);
  AssignInputValues(B, 5.2f);

  vector<int> static_input_indexes = {};
  auto R = ops::Maximum(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Maximum", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op MaximumBroadcasting

// Test op: Negate
TEST(MathOps, Negate) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 16.5f);

  vector<int> static_input_indexes = {};
  auto R = ops::Negate(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Neg", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of Test op Negate

// Test op: Pow
TEST(MathOps, Pow1D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 4;
  Tensor A(DT_FLOAT, TensorShape({dim1}));
  Tensor B(DT_FLOAT, TensorShape({dim1}));
  AssignInputValues(A, 1.4f);
  AssignInputValues(B, 0.5f);
  vector<int> static_input_indexes = {};
  auto R = ops::Pow(root, A, B);
  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Pow", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}

TEST(MathOps, Pow2D) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 3;
  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValues(A, -2.5f);
  AssignInputValues(B, 4.0f);
  vector<int> static_input_indexes = {};
  auto R = ops::Pow(root, A, B);
  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Pow", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}

// Broadcasting
TEST(MathOps, Pow0D1D) {
  Scope root = Scope::NewRootScope();
  Tensor A(DT_FLOAT, TensorShape({}));   // scalar == rank 0 (no axes)
  Tensor B(DT_FLOAT, TensorShape({5}));  // vector == rank 1 (1 axis)
  AssignInputValues(A, 2.1f);
  AssignInputValues(B, 4.1f);
  vector<int> static_input_indexes = {};
  auto R = ops::Pow(root, A, B);
  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Pow", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Pow

// Test op: RealDiv
TEST(MathOps, RealDiv) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::RealDiv(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "RealDiv", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op RealDiv

// Test op: RealDivBroadcasting
TEST(MathOps, RealDivBroadcasting) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1}));

  AssignInputValues(A, 2.0f);
  AssignInputValues(B, 7.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::RealDiv(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "RealDiv", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op RealDivBroadcasting

// Test op: RealDiv for nan, inf case
TEST(MathOps, RealDivNonfinite) {
  Scope root = Scope::NewRootScope();
  int dim = 3;

  Tensor A(DT_FLOAT, TensorShape({dim}));
  Tensor B(DT_FLOAT, TensorShape({dim}));

  auto inf = std::numeric_limits<float>::infinity();

  vector<float> dividend_vals = {0, -inf, inf};
  vector<float> divisor_vals = {0, 1.0, 1.0};

  AssignInputValues(A, dividend_vals);
  AssignInputValues(B, divisor_vals);

  vector<int> static_input_indexes = {};
  auto R = ops::RealDiv(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "RealDiv", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test RealDivNonfinite

// Test op: Reciprocal
TEST(MathOps, Reciprocal) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 2.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::Reciprocal(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Reciprocal", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Reciprocal

// Test op: Relu
TEST(MathOps, Relu) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 2.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::Relu(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Relu", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Relu

// Test op: Rsqrt
TEST(MathOps, Rsqrt) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 4.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::Rsqrt(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Rsqrt", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Rsqrt

// Test op: Sign
TEST(MathOps, Sign) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  AssignInputValuesRandom<float>(A, -50, 50);

  vector<int> static_input_indexes = {};
  auto R = ops::Sign(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Sign", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Sign

// Test op: Sin
TEST(MathOps, Sin) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 5;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues<float>(
      A, {0, -0, M_PI / 2, M_PI, 1.0, 3.8, 4.2, -3.9, -4.2, -1.0});

  vector<int> static_input_indexes = {};
  auto R = ops::Sin(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Sin", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Sin

// Test op: Sinh
TEST(MathOps, Sinh) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 5;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues<float>(
      A, {0, -0, M_PI / 2, M_PI, 1.0, 3.8, 4.2, -3.9, -4.2, -1.0});

  vector<int> static_input_indexes = {};
  auto R = ops::Sinh(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Sinh", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Sinh

// Test op: Square
TEST(MathOps, Square) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 4.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::Square(root, A);
  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Square", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Square

// Test op: SqueezeNoAttributes
TEST(MathOps, SqueezeNoAttributes) {
  vector<vector<int64>> shape_vector;
  shape_vector.push_back({1, 10, 2, 3});
  shape_vector.push_back({2, 2, 3, 4});
  shape_vector.push_back({10, 1, 5, 1});
  shape_vector.push_back({1, 1, 1, 1});

  vector<int> static_input_indexes = {};
  vector<DataType> output_datatypes = {DT_INT32};

  for (auto shape : shape_vector) {
    Scope root = Scope::NewRootScope();

    Tensor input(DT_INT32, TensorShape(shape));
    AssignInputValuesRandom<int32>(input, -50, 50);

    auto R = ops::Squeeze(root, input);

    std::vector<Output> sess_run_fetchoutputs = {R};
    OpExecuter opexecuter(root, "Squeeze", static_input_indexes,
                          output_datatypes, sess_run_fetchoutputs);

    opexecuter.RunTest();
  }
}  // end of test op SqueezeNoAttributes

// Test op: SqueezeWithAttributes
TEST(MathOps, SqueezeWithAttributes) {
  // construct a map to store input shape and squeeze dimension attributes
  map<vector<int64>, gtl::ArraySlice<int>> shape_attributes_map;
  shape_attributes_map.insert(
      pair<vector<int64>, gtl::ArraySlice<int>>({1, 10, 2, 3}, {0}));
  shape_attributes_map.insert(
      pair<vector<int64>, gtl::ArraySlice<int>>({10, 1, 5, 1}, {-1, -3}));
  shape_attributes_map.insert(
      pair<vector<int64>, gtl::ArraySlice<int>>({1, 1, 1, 1}, {-1, -2}));
  shape_attributes_map.insert(
      pair<vector<int64>, gtl::ArraySlice<int>>({1, 1, 1, 1}, {0, 1, -2, -3}));

  vector<int> static_input_indexes = {};
  vector<DataType> output_datatypes = {DT_FLOAT};

  for (auto itr : shape_attributes_map) {
    Scope root = Scope::NewRootScope();

    auto input_shape = itr.first;
    auto squeeze_dim = itr.second;

    Tensor input(DT_FLOAT, TensorShape(input_shape));
    AssignInputValuesRandom<float>(input, -50, 50);

    auto attrs = ops::Squeeze::Attrs();
    attrs.axis_ = squeeze_dim;

    auto R = ops::Squeeze(root, input, attrs);

    std::vector<Output> sess_run_fetchoutputs = {R};
    OpExecuter opexecuter(root, "Squeeze", static_input_indexes,
                          output_datatypes, sess_run_fetchoutputs);

    opexecuter.RunTest();
  }
}  // end of test op SqueezeWithAttributes

// Test op: Sqrt
TEST(MathOps, Sqrt) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;
  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 4.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::Sqrt(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Sqrt", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Sqrt

// Test op: SquareDifference
TEST(MathOps, SquaredDifference) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;
  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 7.5f);
  AssignInputValues(B, 5.2f);

  vector<int> static_input_indexes = {};
  auto R = ops::SquaredDifference(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};

  OpExecuter opexecuter(root, "SquaredDifference", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op SquaredDifference

// Test op: SquaredDifferenceBroadcasting
TEST(MathOps, SquaredDifferenceBroadcasting) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1}));

  AssignInputValues(A, 7.5f);
  AssignInputValues(B, 5.2f);

  vector<int> static_input_indexes = {};
  auto R = ops::SquaredDifference(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "SquaredDifference", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op SquaredDifferenceBroadcasting

// Test op: Xdivy
TEST(MathOps, Xdivy) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 4.3f);
  AssignInputValues(B, 3.7f);

  vector<int> static_input_indexes = {};
  auto R = ops::Xdivy(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Xdivy", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}

TEST(MathOps, XdivyZeroX) {
  Scope root = Scope::NewRootScope();
  int dim1 = 3;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, std::vector<float>{0.0f, 1.1f, 5.1f, 3.2f, 8.1f, 1.0f,
                                          -1.0f, 2.0f, 0.0f});
  AssignInputValues(B, std::vector<float>{2.0f, 1.2f, 4.2f, 8.9f, 0.0f, 0.0f,
                                          0.0f, 0.0f, 0.0f});

  vector<int> static_input_indexes = {};
  auto R = ops::Xdivy(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Xdivy", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}

TEST(MathOps, XdivyZeroXZeroY) {
  Scope root = Scope::NewRootScope();

  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 0.0f);
  AssignInputValues(B, 0.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::Xdivy(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Xdivy", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);
  opexecuter.RunTest();
}  // end of test op Xdivy

// Test op: Tan
TEST(MathOps, Tan) {
  Scope root = Scope::NewRootScope();

  int dim1 = 2;
  int dim2 = 3;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 3.5f);

  vector<int> static_input_indexes = {};
  auto R = ops::Tan(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Tan", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Tan

// Test op: Tanh
TEST(MathOps, Tanh) {
  Scope root = Scope::NewRootScope();

  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 7.5f);

  vector<int> static_input_indexes = {};
  auto R = ops::Tanh(root, A);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Tanh", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Tanh

// Test op: UnsortedSegmentSum
TEST(MathOps, UnsortedSegmentSum) {
  Scope root = Scope::NewRootScope();
  Tensor A(DT_FLOAT, TensorShape({3, 4}));
  Tensor B(DT_INT32, TensorShape({3}));
  Tensor C(DT_INT32, TensorShape({}));

  AssignInputValues(A, std::vector<float>{1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f,
                                          8.f, 4.f, 3.f, 2.f, 1.f});
  AssignInputValues(B, std::vector<int>{0, 1, 0});
  AssignInputValues(C, 2);

  vector<int> static_input_indexes = {2};
  auto R = ops::UnsortedSegmentSum(root, A, B, C);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "UnsortedSegmentSum", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op UnsortedSegmentSum

// Test op: UnsortedSegmentSum
TEST(MathOps, UnsortedSegmentSumIdxRange) {
  Scope root = Scope::NewRootScope();
  Tensor A(DT_FLOAT, TensorShape({4, 4, 3}));
  Tensor B(DT_INT32, TensorShape({4}));
  Tensor C(DT_INT32, TensorShape({}));

  AssignInputValuesRandom(A);
  AssignInputValues(B, std::vector<int>{0, 1, 2, 3});
  AssignInputValues(C, 4);

  vector<int> static_input_indexes = {2};
  auto R = ops::UnsortedSegmentSum(root, A, B, C);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "UnsortedSegmentSum", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op UnsortedSegmentSum

// Test op: UnsortedSegmentSum
TEST(MathOps, UnsortedSegmentSumMissingIndices) {
  Scope root = Scope::NewRootScope();
  Tensor A(DT_FLOAT, TensorShape({5, 4, 3}));
  Tensor B(DT_INT32, TensorShape({5}));
  Tensor C(DT_INT32, TensorShape({}));

  AssignInputValuesRandom(A);
  AssignInputValues(B, std::vector<int>{0, 1, 3, 4, 0});
  AssignInputValues(C, 5);

  vector<int> static_input_indexes = {2};
  auto R = ops::UnsortedSegmentSum(root, A, B, C);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "UnsortedSegmentSum", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op UnsortedSegmentSum

// Test op: UnsortedSegmentSum
TEST(MathOps, UnsortedSegmentSumSingleIndex) {
  Scope root = Scope::NewRootScope();
  Tensor A(DT_FLOAT, TensorShape({5, 4, 3}));
  Tensor B(DT_INT32, TensorShape({5}));
  Tensor C(DT_INT32, TensorShape({}));

  AssignInputValuesRandom(A);
  AssignInputValues(B, std::vector<int>{0, 0, 0, 0, 0});
  AssignInputValues(C, 1);

  vector<int> static_input_indexes = {2};
  auto R = ops::UnsortedSegmentSum(root, A, B, C);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "UnsortedSegmentSum", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op UnsortedSegmentSum

// Test op: UnsortedSegmentSum
TEST(MathOps, UnsortedSegmentSumTwoDims) {
  Scope root = Scope::NewRootScope();
  Tensor A(DT_FLOAT, TensorShape({2, 3, 3}));
  Tensor B(DT_INT32, TensorShape({2, 3}));
  Tensor C(DT_INT32, TensorShape({}));

  AssignInputValuesRandom(A);
  AssignInputValues(B, std::vector<int>{0, 1, 0, 1, 0, 1});
  AssignInputValues(C, 2);

  vector<int> static_input_indexes = {2};
  auto R = ops::UnsortedSegmentSum(root, A, B, C);

  vector<DataType> output_datatypes = {DT_FLOAT};

  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "UnsortedSegmentSum", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op UnsortedSegmentSum

// Test op: NotEqual
TEST(MathOps, NotEqual) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 2.1f);
  AssignInputValues(B, 4.1f);

  vector<int> static_input_indexes = {};
  auto R = ops::NotEqual(root, A, B);

  vector<DataType> output_datatypes = {DT_BOOL};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "NotEqual", static_input_indexes,
                        output_datatypes, sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op NotEqual

// Test op: Mod
TEST(MathOps, Mod) {
  Scope root = Scope::NewRootScope();
  int dim1 = 2;
  int dim2 = 2;

  Tensor A(DT_FLOAT, TensorShape({dim1, dim2}));
  Tensor B(DT_FLOAT, TensorShape({dim1, dim2}));

  AssignInputValues(A, 4.1f);
  AssignInputValues(B, 2.0f);

  vector<int> static_input_indexes = {};
  auto R = ops::Mod(root, A, B);

  vector<DataType> output_datatypes = {DT_FLOAT};
  std::vector<Output> sess_run_fetchoutputs = {R};
  OpExecuter opexecuter(root, "Mod", static_input_indexes, output_datatypes,
                        sess_run_fetchoutputs);

  opexecuter.RunTest();
}  // end of test op Mod

}  // namespace testing
}  // namespace ngraph_bridge
}
