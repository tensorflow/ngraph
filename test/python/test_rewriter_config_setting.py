# ==============================================================================
#  Copyright 2019-2020 Intel Corporation
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
"""nGraph TensorFlow bridge test for checking backend setting using rewriter config for grappler

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import pytest
import os
import numpy as np
import shutil
import tensorflow as tf
from tensorflow.core.protobuf import rewriter_config_pb2
import ngraph_bridge

from common import NgraphTest


class TestRewriterConfigBackendSetting(NgraphTest):

    @pytest.mark.skipif(
        not ngraph_bridge.is_grappler_enabled(),
        reason='Rewriter config only works for grappler path')
    @pytest.mark.parametrize(("backend",), (
        ('CPU',),
        ('INTERPRETER',),
    ))
    def test_config_updater_api(self, backend):
        dim1 = 3
        dim2 = 4
        a = tf.placeholder(tf.float32, shape=(dim1, dim2), name='a')
        x = tf.placeholder(tf.float32, shape=(dim1, dim2), name='x')
        b = tf.placeholder(tf.float32, shape=(dim1, dim2), name='y')
        axpy = (a * x) + b

        config = tf.ConfigProto()
        rewriter_options = rewriter_config_pb2.RewriterConfig()
        rewriter_options.meta_optimizer_iterations = (
            rewriter_config_pb2.RewriterConfig.ONE)
        rewriter_options.min_graph_nodes = -1
        ngraph_optimizer = rewriter_options.custom_optimizers.add()
        ngraph_optimizer.name = "ngraph-optimizer"
        ngraph_optimizer.parameter_map["ngraph_backend"].s = backend.encode()
        ngraph_optimizer.parameter_map["device_id"].s = b'0'
        # TODO: This test will pass if grappler fails silently.
        # Need to do something about that
        backend_extra_params_map = {
            'CPU': {
                'device_config': ''
            },
            'INTERPRETER': {
                'test_echo': '42',
                'hello': '3'
            }
        }
        extra_params = backend_extra_params_map[backend]
        for k in extra_params:
            ngraph_optimizer.parameter_map[k].s = extra_params[k].encode()
        config.MergeFrom(
            tf.ConfigProto(
                graph_options=tf.GraphOptions(
                    rewrite_options=rewriter_options)))

        with tf.Session(config=config) as sess:
            outval = sess.run(
                axpy,
                feed_dict={
                    a: 1.5 * np.ones((dim1, dim2)),
                    b: np.ones((dim1, dim2)),
                    x: np.ones((dim1, dim2))
                })
        assert (outval == 2.5 * (np.ones((dim1, dim2)))).all()
