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

#include <cstring>
#include <memory>
#include <utility>

#include "ngraph/ngraph.hpp"

#include "ie_tensor.h"

using namespace ngraph;
using namespace std;

namespace tensorflow {
namespace ngraph_bridge {

IETensor::IETensor(const element::Type& element_type, const PartialShape& shape)
    : runtime::Tensor(
          make_shared<descriptor::Tensor>(element_type, shape, "")) {
  m_descriptor->set_tensor_layout(
      make_shared<descriptor::layout::DenseTensorLayout>(*m_descriptor));
}

IETensor::IETensor(const element::Type& element_type, const Shape& shape)
    : runtime::Tensor(make_shared<descriptor::Tensor>(element_type, shape, "")),
      m_data(shape_size(shape) * element_type.size()) {
  m_descriptor->set_tensor_layout(
      make_shared<descriptor::layout::DenseTensorLayout>(*m_descriptor));
}

void IETensor::write(const void* src, size_t bytes) {
  const int8_t* src_ptr = static_cast<const int8_t*>(src);
  if (src_ptr == nullptr) {
    return;
  }
  if (get_partial_shape().is_dynamic()) {
    m_data = move(ngraph::runtime::AlignedBuffer(bytes));
  }
  NGRAPH_CHECK(m_data.size() <= bytes, "Buffer over-write. The buffer size: ",
               m_data.size(), " is lower than the number of bytes to write: ",
               bytes);
  copy(src_ptr, src_ptr + bytes, m_data.get_ptr<int8_t>());
}

void IETensor::read(void* dst, size_t bytes) const {
  int8_t* dst_ptr = static_cast<int8_t*>(dst);
  if (dst_ptr == nullptr) {
    return;
  }
  NGRAPH_CHECK(bytes <= m_data.size(),
               "Buffer over-read. The amount of bytes to read: ", bytes,
               " is greater than the size of buffer: ", m_data.size());
  copy(m_data.get_ptr<int8_t>(), m_data.get_ptr<int8_t>() + bytes, dst_ptr);
}

const void* IETensor::get_data_ptr() const { return m_data.get_ptr(); }
}
}