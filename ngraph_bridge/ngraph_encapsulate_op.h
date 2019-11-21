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

#ifndef NGRAPH_TF_ENCAPSULATE_OP_H_
#define NGRAPH_TF_ENCAPSULATE_OP_H_
#pragma once

#include <ostream>
#include <vector>

#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/graph/graph.h"

#include "logging/ngraph_log.h"
#include "ngraph/ngraph.hpp"
#include "ngraph_bridge/ngraph_api.h"
#include "ngraph_bridge/ngraph_encapsulate_impl.h"
#include "ngraph_bridge/ngraph_freshness_tracker.h"

namespace tensorflow {

namespace ngraph_bridge {

class NGraphEncapsulateOp : public OpKernel {
 public:
  explicit NGraphEncapsulateOp(OpKernelConstruction* ctx);
  ~NGraphEncapsulateOp() override;
  void Compute(OpKernelContext* ctx) override;

 private:
  NGraphEncapsulateImpl ng_encap_impl;
  std::mutex m_compute_lock;
};

}  // namespace ngraph_bridge

}  // namespace tensorflow
#endif  // NGRAPH_TF_ENCAPSULATE_OP_H_
