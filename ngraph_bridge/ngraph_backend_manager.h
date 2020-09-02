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

// The backend manager class is a singelton class that interfaces with the
// bridge to provide necessary backend

#ifndef NGRAPH_TF_BRIDGE_BACKEND_MANAGER_H_
#define NGRAPH_TF_BRIDGE_BACKEND_MANAGER_H_

#include <atomic>
#include <mutex>
#include <ostream>
#include <vector>

#include "tensorflow/core/lib/core/errors.h"

#include "ngraph/ngraph.hpp"

#include "logging/ngraph_log.h"
#include "ngraph_bridge/ngraph_backend.h"

using namespace std;

namespace tensorflow {
namespace ngraph_bridge {

class BackendManager {
 public:
  // Returns the nGraph supported backend names
  static vector<string> GetSupportedBackends();

  // Set the BackendManager backend ng_backend_name_
  static Status SetBackend(const string& backend_name = "CPU");

  // Returns the currently set backend
  static shared_ptr<Backend> GetBackend();

  // Returns the currently set backend's name
  static Status GetBackendName(string& backend_name);

  static void SetConfig(const map<string, string>& config);

  ~BackendManager();

 private:
  // Creates backend of backend_name type
  static Status CreateBackend(shared_ptr<Backend>& backend,
                              string& backend_name);

  static shared_ptr<Backend> m_backend;
  static string m_backend_name;
  static mutex m_backend_mutex;
};

}  // namespace ngraph_bridge
}  // namespace tensorflow

#if defined(NGRAPH_BRIDGE_STATIC_LIB_ENABLE)
extern "C" void ngraph_register_cpu_backend();
extern "C" void ngraph_register_interpreter_backend();
#endif

#endif
// NGRAPH_TF_BRIDGE_BACKEND_MANAGER_H
