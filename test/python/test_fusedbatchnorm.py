# ==============================================================================
#  Copyright 2018-2020 Intel Corporation
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
"""nGraph TensorFlow FusedBatchNorm test

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np
import pytest
import tensorflow as tf

from common import NgraphTest

# yes, it works without (tested over 1000 runs) but there's always a chance
np.random.seed(5)

NHWC_TO_NCHW = (0, 3, 1, 2)
NCHW_TO_NHWC = (0, 2, 3, 1)


class TestFusedBatchNorm(NgraphTest):
    x = np.random.rand(64, 3, 10, 8).astype('f')  #NCHW
    scale = [1.0, 0.9, 1.1]
    offset = [0.1, 0.2, -.3]
    mean = [0.4, 0.5, 0.6]
    variance = [0.1, 0.2, 0.3]

    def test_fusedbatchnorm_nchw(self):

        def test_on_ng(sess):
            norm = tf.nn.fused_batch_norm(
                self.x, self.scale, self.offset, data_format='NCHW')
            return (sess.run(norm))

        def test_on_tf(sess):
            # tensorflow CPU doesn't support NCHW
            x_t = tf.transpose(self.x, NCHW_TO_NHWC)  # NHWC
            norm = tf.nn.fused_batch_norm(
                x_t, self.scale, self.offset, data_format='NHWC')
            return (sess.run(norm))

        expected = self.without_ngraph(test_on_tf)
        result = self.with_ngraph(test_on_ng)
        np.testing.assert_allclose(
            result[0],
            np.transpose(expected[0], NHWC_TO_NCHW),
            rtol=0,
            atol=5e-5)
        np.testing.assert_allclose(result[1], expected[1], rtol=0, atol=5e-5)
        np.testing.assert_allclose(result[2], expected[2], rtol=0, atol=5e-5)

    def test_fusedbatchnorm_nhwc(self):
        x_t = tf.transpose(self.x, NCHW_TO_NHWC)

        def test_on_ng(sess):
            norm = tf.nn.fused_batch_norm(
                x_t, self.scale, self.offset, data_format='NHWC')
            return (sess.run(norm))

        def test_on_tf(sess):
            norm = tf.nn.fused_batch_norm(
                x_t, self.scale, self.offset, data_format='NHWC')
            return (sess.run(norm))

        expected = self.without_ngraph(test_on_tf)
        result = self.with_ngraph(test_on_ng)
        np.testing.assert_allclose(result[0], expected[0], rtol=0, atol=5e-5)
        np.testing.assert_allclose(result[1], expected[1], rtol=0, atol=5e-5)
        np.testing.assert_allclose(result[2], expected[2], rtol=0, atol=5e-5)

    def test_fusedbatchnorm_inference_nchw(self):

        def test_on_ng(sess):
            norm = tf.nn.fused_batch_norm(
                self.x,
                self.scale,
                self.offset,
                self.mean,
                self.variance,
                data_format='NCHW',
                is_training=False)
            return (sess.run(norm[0]))

        def test_on_tf(sess):
            x_t = tf.transpose(self.x, NCHW_TO_NHWC)
            norm = tf.nn.fused_batch_norm(
                x_t,
                self.scale,
                self.offset,
                self.mean,
                self.variance,
                data_format='NHWC',
                is_training=False)
            return (sess.run(norm[0]))

        expected = self.without_ngraph(test_on_tf)
        result = self.with_ngraph(test_on_ng)
        np.testing.assert_allclose(
            result, np.transpose(expected, NHWC_TO_NCHW), rtol=0, atol=5e-5)

    def test_fusedbatchnorm_inference_nhwc(self):
        x_t = tf.transpose(self.x, NCHW_TO_NHWC)

        def test_on_ng(sess):
            norm = tf.nn.fused_batch_norm(
                x_t,
                self.scale,
                self.offset,
                self.mean,
                self.variance,
                data_format='NHWC',
                is_training=False)
            return (sess.run(norm[0]))

        def test_on_tf(sess):
            norm = tf.nn.fused_batch_norm(
                x_t,
                self.scale,
                self.offset,
                self.mean,
                self.variance,
                data_format='NHWC',
                is_training=False)
            return (sess.run(norm[0]))

        np.testing.assert_allclose(
            self.with_ngraph(test_on_ng),
            self.without_ngraph(test_on_tf),
            rtol=0,
            atol=5e-5)
