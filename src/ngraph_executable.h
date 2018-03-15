/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_COMPILER_NGRAPH_DRIVER_NGRAPH_EXECUTABLE_H_
#define TENSORFLOW_COMPILER_NGRAPH_DRIVER_NGRAPH_EXECUTABLE_H_

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "ngraph/runtime/external_function.hpp"
#include "ngraph/runtime/manager.hpp"
#include "ngraph/runtime/tensor_view.hpp"
#include "tensorflow/compiler/xla/service/executable.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_module_config.h"
#include "tensorflow/stream_executor/lib/status.h"
#include "tensorflow/stream_executor/lib/statusor.h"

namespace se = ::perftools::gputools;

namespace xla {
namespace ngraph_plugin {

class NGraphExecutable : public Executable {
 public:
  NGraphExecutable(
      std::unique_ptr<HloModule> hlo_module,
      std::shared_ptr<ngraph::runtime::Manager> ng_manager,
      std::shared_ptr<ngraph::runtime::ExternalFunction> ng_runtime_function);
  ~NGraphExecutable() override;

  // StatusOr<se::DeviceMemoryBase>
  // ExecuteOnStream(const ServiceExecutableRunOptions *run_options,
  //                tensorflow::gtl::ArraySlice<se::DeviceMemoryBase> arguments,
  //                HloExecutionProfile *hlo_execution_profile) override;

  StatusOr<std::unique_ptr<ShapedBuffer>> ExecuteOnStream(
      const ServiceExecutableRunOptions* run_options,
      tensorflow::gtl::ArraySlice<const ShapedBuffer*> arguments,
      HloExecutionProfile* hlo_execution_profile) override;

  // StatusOr<se::DeviceMemoryBase> ExecuteAsyncOnStream(
  //    const ServiceExecutableRunOptions *run_options,
  //    tensorflow::gtl::ArraySlice<se::DeviceMemoryBase> arguments) override;

  StatusOr<std::unique_ptr<ShapedBuffer>> ExecuteAsyncOnStream(
      const ServiceExecutableRunOptions* run_options,
      tensorflow::gtl::ArraySlice<const ShapedBuffer*> arguments) override;

  static int64 ShapeSizeBytes(const Shape& shape);

 private:
  Status CreateInputArgs(
      const xla::HloComputation* entry_computation,
      std::shared_ptr<ngraph::runtime::Backend>& ng_backend,
      tensorflow::gtl::ArraySlice<const ShapedBuffer*> arguments,
      std::vector<std::shared_ptr<ngraph::runtime::TensorView>>& ng_arg_list);

  StatusOr<std::shared_ptr<ngraph::runtime::TensorView>> CreateNGraphTensor(
      const xla::Shape& xla_shape,
      const std::shared_ptr<ngraph::runtime::Backend>& ng_backend);

  std::shared_ptr<ngraph::runtime::Manager> m_ng_manager;
  std::shared_ptr<ngraph::runtime::ExternalFunction> m_ng_runtime_function;

  TF_DISALLOW_COPY_AND_ASSIGN(NGraphExecutable);
};

}  // namespace ngraph_plugin
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_NGRAPH_DRIVER_NGRAPH_EXECUTABLE_H_
