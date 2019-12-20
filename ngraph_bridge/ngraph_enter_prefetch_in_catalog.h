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
#ifndef NGRAPH_TF_ENTER_PREFETCH_IN_CATALOG_H_
#define NGRAPH_TF_ENTER_PREFETCH_IN_CATALOG_H_
#pragma once

#include "tensorflow/core/graph/graph.h"

#include "ngraph/ngraph.hpp"

#include "ngraph_bridge/ngraph_catalog.h"

using namespace std;
namespace ng = ngraph;

namespace tensorflow {

namespace ngraph_bridge {

// Populate the NGraphCatalog for Prefetched Inputs
Status EnterPrefetchInCatalog(Graph* graph, int graph_id);

}  // ngraph_bridge
}  // tensorflow

#endif
