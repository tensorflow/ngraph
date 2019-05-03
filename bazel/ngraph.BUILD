# ==============================================================================
#  Copyright 2019 Intel Corporation
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
# ==============================================================================
licenses(["notice"])  
exports_files(["LICENSE"])

load("@ngraph_bridge//:cxx_abi_option.bzl", "CXX_ABI")

cc_library(
    name = "ngraph_headers",
    hdrs = glob([
        "src/ngraph/**/*.hpp",
        "src/ngraph/*.hpp"
    ]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ngraph_core",
    srcs = glob([
        "src/ngraph/*.cpp",
        "src/ngraph/autodiff/*.cpp",
        "src/ngraph/builder/*.cpp",
        "src/ngraph/descriptor/*.cpp",
        "src/ngraph/descriptor/layout/*.cpp",
        "src/ngraph/op/*.cpp",
        "src/ngraph/op/fused/*.cpp",
        "src/ngraph/op/experimental/dyn_broadcast.cpp",
        "src/ngraph/op/experimental/dyn_pad.cpp",
        "src/ngraph/op/experimental/dyn_reshape.cpp",
        "src/ngraph/op/experimental/dyn_slice.cpp",
        "src/ngraph/op/experimental/generate_mask.cpp",
        "src/ngraph/op/experimental/quantized_avg_pool.cpp",
        "src/ngraph/op/experimental/quantized_concat.cpp",
        "src/ngraph/op/experimental/quantized_conv.cpp",
        "src/ngraph/op/experimental/quantized_conv_bias.cpp",
        "src/ngraph/op/experimental/quantized_conv_relu.cpp",
        "src/ngraph/op/experimental/quantized_max_pool.cpp",
        "src/ngraph/op/experimental/shape_of.cpp",
        "src/ngraph/op/experimental/quantized_dot.cpp",
        "src/ngraph/op/experimental/quantized_dot_bias.cpp",
        "src/ngraph/op/experimental/transpose.cpp",
        "src/ngraph/op/util/*.cpp",
        "src/ngraph/pattern/*.cpp",
        "src/ngraph/pattern/*.hpp",
        "src/ngraph/pass/*.cpp",
        "src/ngraph/pass/*.hpp",
        "src/ngraph/runtime/*.cpp",
        "src/ngraph/type/*.cpp",
        ],
        exclude = [
        "src/ngraph/ngraph.cpp",
    ]),
    deps = [
        ":ngraph_headers",
        "@nlohmann_json_lib",
    ],
    copts = [
        "-I external/ngraph/src",
        "-I external/ngraph/src/ngraph",
        "-I external/nlohmann_json_lib/include/",
        "-D_FORTIFY_SOURCE=2",
        "-Wformat",
        "-Wformat-security",
        "-Wformat",
        "-fstack-protector-all",
        '-D SHARED_LIB_PREFIX=\\"lib\\"',
        '-D SHARED_LIB_SUFFIX=\\".so\\"',
        '-D NGRAPH_VERSION=\\"0.19.0-rc.1\\"',
        "-D NGRAPH_DEX_ONLY",
        '-D PROJECT_ROOT_DIR=\\"\\"',
    ] + CXX_ABI,
    linkopts = [
        "-Wl,-z,noexecstack",
        "-Wl,-z,relro",
        "-Wl,-z,now",
    ],
    visibility = ["//visibility:public"],
    alwayslink = 1,
)

cc_binary(
    name = 'libinterpreter_backend.so',
    srcs = glob([
        "src/ngraph/except.hpp",
        "src/ngraph/runtime/interpreter/*.cpp",
        "src/ngraph/state/rng_state.cpp",
        "src/ngraph/runtime/hybrid/hybrid_backend.cpp",
        "src/ngraph/runtime/hybrid/hybrid_backend.hpp",
        "src/ngraph/runtime/hybrid/hybrid_executable.cpp",
        "src/ngraph/runtime/hybrid/hybrid_executable.hpp",
        "src/ngraph/runtime/hybrid/hybrid_util.cpp",
        "src/ngraph/runtime/hybrid/hybrid_util.hpp",
        "src/ngraph/runtime/hybrid/op/function_call.cpp",
        "src/ngraph/runtime/hybrid/op/function_call.hpp",
        "src/ngraph/runtime/hybrid/pass/default_placement.cpp",
        "src/ngraph/runtime/hybrid/pass/default_placement.hpp",
        "src/ngraph/runtime/hybrid/pass/dump.cpp",
        "src/ngraph/runtime/hybrid/pass/dump.hpp",
        "src/ngraph/runtime/hybrid/pass/fix_get_output_element.cpp",
        "src/ngraph/runtime/hybrid/pass/fix_get_output_element.hpp",
        "src/ngraph/runtime/hybrid/pass/liveness.cpp",
        "src/ngraph/runtime/hybrid/pass/liveness.hpp",
        "src/ngraph/runtime/hybrid/pass/memory_layout.cpp",
        "src/ngraph/runtime/hybrid/pass/memory_layout.hpp",        
    ]),
    deps = [
        ":ngraph_headers",
        ":ngraph_core",
    ],
    copts = [
        "-I external/ngraph/src",
        "-I external/ngraph/src/ngraph",
        "-I external/nlohmann_json_lib/include/",
        "-D_FORTIFY_SOURCE=2",
        "-Wformat",
        "-Wformat-security",
        "-Wformat",
        "-fstack-protector-all",
        '-D SHARED_LIB_PREFIX=\\"lib\\"',
        '-D SHARED_LIB_SUFFIX=\\".so\\"',
        '-D NGRAPH_VERSION=\\"0.19.0-rc.1\\"',
        "-D NGRAPH_DEX_ONLY",
        '-D PROJECT_ROOT_DIR=\\"\\"',
    ] + CXX_ABI,
    linkopts = [
        "-Wl,-z,noexecstack",
        "-Wl,-z,relro",
        "-Wl,-z,now",
    ],
    linkshared = 1,
    visibility = ["//visibility:public"],
)
 
cc_library(
    name = 'ngraph_version',
    srcs = glob([
        "src/ngraph/ngraph.cpp"
    ]),
    deps = [
        ":ngraph_headers",
    ],
    copts = [
        "-I external/ngraph/src",
        "-I external/ngraph/src/ngraph",
        "-I external/nlohmann_json_lib/include/",
        "-D_FORTIFY_SOURCE=2",
        "-Wformat",
        "-Wformat-security",
        "-Wformat",
        "-fstack-protector-all",
        '-D SHARED_LIB_PREFIX=\\"lib\\"',
        '-D SHARED_LIB_SUFFIX=\\".so\\"',
        '-D NGRAPH_VERSION=\\"0.19.0-rc.1\\"',
        "-D NGRAPH_DEX_ONLY",
        '-D PROJECT_ROOT_DIR=\\"\\"',
    ] + CXX_ABI,
    linkopts = [
        "-Wl,-z,noexecstack",
        "-Wl,-z,relro",
        "-Wl,-z,now",
    ],
    visibility = ["//visibility:public"],
    alwayslink = 1,
)

