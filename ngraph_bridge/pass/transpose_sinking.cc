//*****************************************************************************
// Copyright 2017-2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#include <algorithm>
#include <iostream>
#include <numeric>
#include <set>
#include <unordered_set>

#include "ngraph/ngraph.hpp"
#include "ngraph/opsets/opset3.hpp"
#include "ngraph/pattern/op/label.hpp"
#include "ngraph/util.hpp"

#include "logging/ngraph_log.h"
#include "ngraph_bridge/pass/transpose_sinking.h"

using namespace std;

namespace tensorflow {
namespace ngraph_bridge {

using TransposeMap = unordered_map<shared_ptr<ngraph::Node>,
                                   shared_ptr<ngraph::opset3::Transpose>>;

static ngraph::CoordinateDiff apply_permutation(ngraph::CoordinateDiff input,
                                                ngraph::AxisVector order) {
  ngraph::CoordinateDiff output(input.size());
  for (size_t i = 0; i < order.size(); i++) {
    output[i] = input.at(order.at(i));
  }
  return output;
}

template <typename T>
static string describe(shared_ptr<ngraph::Node> node) {
  // ensure that it's either a reshape or a transpose
  // TODO: use static_assert
  if (!(std::is_base_of<ngraph::opset3::Reshape, T>::value ||
        std::is_base_of<ngraph::opset3::Transpose, T>::value)) {
    throw runtime_error(
        "describe template specialization has to be either reshape or "
        "transpose");
  }
  stringstream ss;
  auto transpose = ngraph::as_type_ptr<T>(node);
  auto const1 = ngraph::as_type_ptr<ngraph::opset3::Constant>(
      transpose->get_input_node_shared_ptr(1));
  ss << transpose->get_name() << " ( axis order = "
     << ngraph::vector_to_string(const1->get_axis_vector_val())
     << " , shape = " << ngraph::vector_to_string(transpose->get_shape())
     << " ) "
     << " , child = " << transpose->get_argument(0)->get_name();
  return ss.str();
}

static shared_ptr<ngraph::opset3::Transpose> make_transpose(
    shared_ptr<ngraph::Node> arg, const ngraph::AxisVector& input_order) {
  auto ng_input_order = std::make_shared<ngraph::opset3::Constant>(
      ngraph::element::u64, ngraph::Shape{input_order.size()}, input_order);
  auto transpose = make_shared<ngraph::opset3::Transpose>(arg, ng_input_order);
  NGRAPH_VLOG(4) << "Make Transpose "
                 << describe<ngraph::opset3::Transpose>(transpose);
  return transpose;
}

static shared_ptr<ngraph::opset3::Reshape> make_reshape(
    shared_ptr<ngraph::Node> arg, const ngraph::AxisVector& input_order) {
  auto ng_input_order = std::make_shared<ngraph::opset3::Constant>(
      ngraph::element::u64, ngraph::Shape{input_order.size()}, input_order);
  auto transpose =
      make_shared<ngraph::opset3::Reshape>(arg, ng_input_order, false);
  NGRAPH_VLOG(4) << "Make Reshape "
                 << describe<ngraph::opset3::Reshape>(transpose);
  return transpose;
}

static void write_transposemap(
    TransposeMap& reorders, shared_ptr<ngraph::Node> target,
    shared_ptr<ngraph::opset3::Transpose> transpose) {
  NGRAPH_VLOG(4) << "Write TransposeMap[" << target->get_name()
                 << "] = " << describe<ngraph::opset3::Transpose>(transpose);
  reorders[target] = transpose;
}

static shared_ptr<ngraph::opset3::Transpose> read_transposemap(
    TransposeMap& reorders, shared_ptr<ngraph::Node> target) {
  auto transpose = reorders.at(target);
  NGRAPH_VLOG(4) << "Read TransposeMap[" << target->get_name() << "]  -> "
                 << describe<ngraph::opset3::Transpose>(transpose);
  return transpose;
}

static shared_ptr<ngraph::opset3::Transpose> combine_transposes(
    shared_ptr<ngraph::opset3::Transpose> t1,
    shared_ptr<ngraph::opset3::Transpose> t2) {
  auto default_order = ngraph::get_default_order(t1->get_shape());
  auto t1_const = ngraph::as_type_ptr<ngraph::opset3::Constant>(
      t1->input_value(1).get_node_shared_ptr());
  auto t2_const = ngraph::as_type_ptr<ngraph::opset3::Constant>(
      t2->input_value(1).get_node_shared_ptr());
  auto perm_t1 =
      ngraph::apply_permutation(default_order, t1_const->get_axis_vector_val());
  auto perm_t2 =
      ngraph::apply_permutation(perm_t1, t2_const->get_axis_vector_val());
  auto combined =
      make_transpose(t2->input_value(0).get_node_shared_ptr(), perm_t2);
  NGRAPH_VLOG(4) << "Combining " << describe<ngraph::opset3::Transpose>(t1)
                 << " and " << describe<ngraph::opset3::Transpose>(t2)
                 << " into " << describe<ngraph::opset3::Transpose>(combined);
  return combined;
}

static void insert_transpose(shared_ptr<ngraph::Node> target,
                             shared_ptr<ngraph::Node> transpose,
                             size_t input_index) {
  NGRAPH_VLOG(4) << "Inserting transpose at input " << target->get_name()
                 << " input index " << input_index;
  auto arg = target->input(input_index).get_source_output();
  NGRAPH_VLOG(4) << "Arg shape: " << arg.get_shape();
  auto new_order = ngraph::as_type_ptr<ngraph::opset3::Constant>(
      transpose->input_value(1).get_node_shared_ptr());
  auto new_transpose = make_transpose(arg.get_node_shared_ptr(),
                                      new_order->get_axis_vector_val());
  NGRAPH_VLOG(4) << "Inserting transpose "
                 << describe<ngraph::opset3::Transpose>(new_transpose)
                 << " at input " << target->get_name() << " input index "
                 << input_index;
  target->input(input_index).replace_source_output(new_transpose->output(0));
}

static void delete_transpose(shared_ptr<ngraph::Node> transpose) {
  NGRAPH_VLOG(4) << "Removing transpose " << transpose->get_name();
  if (!transpose->get_users().empty()) {
    ngraph::replace_node(transpose, transpose->get_argument(0));
  }
}

static void mark_transpose_for_deletion(
    shared_ptr<ngraph::Node> transpose,
    set<shared_ptr<ngraph::Node>>& transposes_to_delete) {
  NGRAPH_VLOG(4) << "Marking transpose " << transpose->get_name()
                 << " for deletion";
  transposes_to_delete.insert(transpose);
}

static shared_ptr<ngraph::opset3::Transpose> create_default_transpose(
    shared_ptr<ngraph::Node> n) {
  auto default_order = ngraph::get_default_order(n->get_shape());
  auto default_transpose = make_transpose(n, default_order);
  NGRAPH_VLOG(4) << "Default transpose: "
                 << describe<ngraph::opset3::Transpose>(default_transpose);
  return default_transpose;
}

// convert_binary_to_default_order is used when one of the arguments
// of a binary op isn't in the default format (i.e. nhwc instead of nchw)
// We have to normalize this other argument to nchw by swimming nchw towards
// parameters
// as far as we can
static void convert_binary_to_default_order(
    shared_ptr<ngraph::Node> binary, const ngraph::Input<ngraph::Node>& input,
    shared_ptr<ngraph::Node> right, TransposeMap& reorders,
    set<shared_ptr<ngraph::Node>>& transposes_to_delete) {
  auto left = input.get_source_output().get_node_shared_ptr();
  auto right_const = ngraph::as_type_ptr<ngraph::opset3::Constant>(
      reorders.at(right)->input_value(1).get_node_shared_ptr());

  auto perm_to_def = ngraph::get_permutation_to_default_order(
      right_const->get_axis_vector_val());

  // if right input is being implicitly broadcasted, insert a reshape
  // instead of a transpose
  shared_ptr<ngraph::Node> new_node;
  auto left_shape = left->get_shape();
  if (left_shape.size() < perm_to_def.size()) {
    left_shape.insert(left_shape.begin(),
                      perm_to_def.size() - left_shape.size(), 1);
    auto new_shape = ngraph::apply_permutation(left_shape, perm_to_def);
    new_node = make_reshape(left, new_shape);
  } else if (left_shape.size() == perm_to_def.size()) {
    new_node = make_transpose(left, perm_to_def);
  } else {
    throw runtime_error(
        "case not supported when converting binary to default order");
  }
  input.replace_source_output(new_node->output(0));

  NGRAPH_VLOG(4) << "right = " << ngraph::vector_to_string(right->get_shape())
                 << ", " << right->get_name();
  // this should now insert transpose on right
  mark_transpose_for_deletion(reorders.at(right), transposes_to_delete);
  write_transposemap(reorders, binary, read_transposemap(reorders, right));
}

static void materialize_shapes(
    shared_ptr<ngraph::Node> n, TransposeMap& reorders,
    set<shared_ptr<ngraph::Node>>& transposes_to_delete) {
  // skip multiple output nodes and deal with GOEs exclusively
  if (n->get_output_size() > 1) {
    return;
  }

  for (size_t i = 0; i < n->get_arguments().size(); i++) {
    // materialize all pending transposes, flush pending transposes
    auto arg = n->get_argument(i);
    if (reorders.count(arg) != 0) {
      auto arg_transpose = reorders.at(arg);
      NGRAPH_VLOG(4) << "Materializing "
                     << describe<ngraph::opset3::Transpose>(arg_transpose)
                     << " for " << arg->get_name();
      mark_transpose_for_deletion(arg_transpose, transposes_to_delete);
      auto arg_shape = arg->get_shape();
      auto arg_transpose_order = ngraph::as_type_ptr<ngraph::opset3::Constant>(
          arg_transpose->input_value(1).get_node_shared_ptr());
      if (arg_transpose_order->get_axis_vector_val() !=
          get_default_order(arg->get_shape())) {
        // Insert if arg needs to be transposed.
        insert_transpose(n, arg_transpose, i);
      }
    }
  }
  write_transposemap(reorders, n, create_default_transpose(n));
}

static void sink_transpose(
    shared_ptr<ngraph::opset3::Transpose> transpose, TransposeMap& reorders,
    set<shared_ptr<ngraph::Node>>& transposes_to_delete) {
  NGRAPH_VLOG(4) << "Sinking Transpose :"
                 << describe<ngraph::opset3::Transpose>(transpose);
  auto orig_transpose = reorders.at(transpose->get_argument(0));
  // combine both transposes
  auto new_transpose = combine_transposes(orig_transpose, transpose);
  // remove original transpose now it's combined with a new one
  // should be safe to remove an already detached node
  mark_transpose_for_deletion(orig_transpose, transposes_to_delete);
  // replace transpose with combined one
  ngraph::replace_node(transpose, new_transpose);
  mark_transpose_for_deletion(new_transpose, transposes_to_delete);
  write_transposemap(reorders, new_transpose, new_transpose);
}

static void sink_unary(
    shared_ptr<ngraph::Node> n, TransposeMap& reorders,
    set<shared_ptr<ngraph::Node>>& /* transposes_to_delete */) {
  auto arg_transpose = read_transposemap(reorders, n->get_argument(0));
  NGRAPH_VLOG(4) << "Propagating "
                 << describe<ngraph::opset3::Transpose>(arg_transpose)
                 << " for " << n->get_name();
  write_transposemap(reorders, n, arg_transpose);
}

static void sink_binary(shared_ptr<ngraph::Node> binary, TransposeMap& reorders,
                        set<shared_ptr<ngraph::Node>>& transposes_to_delete) {
  auto left = binary->get_argument(0);
  auto right = binary->get_argument(1);

  auto left_const = ngraph::as_type_ptr<ngraph::opset3::Constant>(
      reorders.at(left)->input_value(1).get_node_shared_ptr());
  auto right_const = ngraph::as_type_ptr<ngraph::opset3::Constant>(
      reorders.at(right)->input_value(1).get_node_shared_ptr());

  auto left_order = left_const->get_axis_vector_val();
  auto right_order = right_const->get_axis_vector_val();

  auto left_mismatch =
      left_order != ngraph::get_default_order(left->get_shape());
  auto right_mismatch =
      right_order != ngraph::get_default_order(right->get_shape());

  NGRAPH_VLOG(4) << "Sink binary " << binary->get_name()
                 << " left transpose: " << ngraph::vector_to_string(left_order)
                 << " left default: "
                 << ngraph::vector_to_string(
                        ngraph::get_default_order(left->get_shape()))
                 << " right transpose: "
                 << ngraph::vector_to_string(right_order) << " right default: "
                 << ngraph::vector_to_string(
                        ngraph::get_default_order(right->get_shape()));

  if ((left_order.size() == right_order.size() && left_order == right_order) ||
      (!left_mismatch && !right_mismatch)) {
    // Propagate the reshape which matches the shape of the binary node
    auto new_transpose =
        (binary->get_output_shape(0) == left->get_shape()) ? left : right;
    NGRAPH_VLOG(4) << "Propagating " << describe<ngraph::opset3::Transpose>(
                                            reorders.at(new_transpose))
                   << " for " << binary->get_name();
    write_transposemap(reorders, binary,
                       read_transposemap(reorders, new_transpose));
    // at this point, both transposes will be eventually removed
    mark_transpose_for_deletion(reorders.at(left), transposes_to_delete);
    mark_transpose_for_deletion(reorders.at(right), transposes_to_delete);
  } else {
    if (right_mismatch) {
      convert_binary_to_default_order(binary, binary->input(0), right, reorders,
                                      transposes_to_delete);
    }
    if (left_mismatch) {
      convert_binary_to_default_order(binary, binary->input(1), left, reorders,
                                      transposes_to_delete);
    }
  }
}

static void sink_pad(
    shared_ptr<ngraph::opset3::Pad> n, TransposeMap& reorders,
    set<shared_ptr<ngraph::Node>>& /* transposes_to_delete */) {
  auto arg_transpose = reorders.at(n->get_argument(0));
  auto arg_transpose_order = ngraph::as_type_ptr<ngraph::opset3::Constant>(
      arg_transpose->input_value(1).get_node_shared_ptr());
  auto order = arg_transpose_order->get_axis_vector_val();
  // we need the correct input shape to produce the right output shape
  // we are going to create a label of the right input shape,
  // so a new pad will have the right shape
  auto def_order = ngraph::get_permutation_to_default_order(order);
  auto input_shape =
      ngraph::apply_permutation(arg_transpose->get_shape(), def_order);
  auto dummy_correct_shape = make_shared<ngraph::pattern::op::Label>(
      arg_transpose->get_element_type(), input_shape);

  auto pad_begin = apply_permutation(n->get_pads_begin(), def_order);
  auto pad_end = apply_permutation(n->get_pads_end(), def_order);
  auto new_begin = make_shared<ngraph::opset3::Constant>(
      ngraph::element::i64, ngraph::Shape{pad_begin.size()}, pad_begin);
  auto new_end = make_shared<ngraph::opset3::Constant>(
      ngraph::element::i64, ngraph::Shape{pad_end.size()}, pad_end);
  auto new_pad =
      make_shared<ngraph::opset3::Pad>(dummy_correct_shape, new_begin, new_end,
                                       n->get_argument(3), n->get_pad_mode());
  ngraph::replace_node(dummy_correct_shape, n->get_argument(0));
  NGRAPH_VLOG(4) << "Replacing " << n->get_name() << " with "
                 << new_pad->get_name();
  ngraph::replace_node(n, new_pad);
  auto new_transpose = make_transpose(new_pad, order);
  NGRAPH_VLOG(4) << "Propagating "
                 << describe<ngraph::opset3::Transpose>(new_transpose)
                 << " for " << n->get_name();
  write_transposemap(reorders, new_pad, new_transpose);
}

// The goal of TransposeSinking is to remove
// round-trip transposes(i.e. nhwc->nchw(nchw-only-op)->nhwc)
// around nchw-only-op (e.g.Convolution, Batchnorm, Avg/MaxPool)
// This is achieved by both **sinking**, propagating transposes
// through ops towards ngraph::op::Results,
// or **swimming** Transposes up towards ngraph::op::Parameter
// For each op type we support we can either combine
// two transposes by replacing the existing Transpose,
// materialize pending transposes if they can't be propagated through op
bool TransposeSinking::run_on_function(shared_ptr<ngraph::Function> f) {
  TransposeMap reorders;
  ngraph::NodeVector results;
  set<shared_ptr<ngraph::Node>> transposes_to_delete;

  // STEP 1 : Sink or Swim transposes away for op clusters
  for (auto n : f->get_ordered_ops()) {
    NGRAPH_VLOG(4) << "Start: Processing node " << n->get_name();
    // collect all Result nodes for a sanity check
    if (n->is_output()) {
      results.push_back(n);
    }

    if (auto transpose = ngraph::as_type_ptr<ngraph::opset3::Transpose>(n)) {
      sink_transpose(transpose, reorders, transposes_to_delete);
    } else if (n->is_unary_elementwise_arithmetic()) {
      sink_unary(n, reorders, transposes_to_delete);
    } else if (n->is_binary_elementwise_arithmetic()) {
      sink_binary(n, reorders, transposes_to_delete);
    } else if (auto pad = ngraph::as_type_ptr<ngraph::opset3::Pad>(n)) {
      sink_pad(pad, reorders, transposes_to_delete);
    } else {
      materialize_shapes(n, reorders, transposes_to_delete);
    }
    NGRAPH_VLOG(4) << "End: Processing node " << n->get_name();
  }

  // STEP 2: purge all the transposes we either sunk or swam.
  for (auto r : transposes_to_delete) {
    delete_transpose(r);
  }

  // STEP 3: fix wrong shape info wholesale
  for (auto n : f->get_ordered_ops()) {
    n->revalidate_and_infer_types();
  }
  return true;
}

}  // namespace ngraph_bridge
}  // namespace tensorflow