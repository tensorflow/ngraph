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
"""nGraph TensorFlow bridge Conv2d operation test

"""
import pytest

import tensorflow as tf
import numpy as np
import os
from tensorflow.python.ops import nn_ops
import ngraph_bridge

#Tests Ngraph Op: ConvolutionBackpropFilters with data format NHWC
#TF Op: conv2d_backprop_filter

np.random.seed(5)
#Inputs
N = 1
H = 7
W = 6
C = 2

I = C
O = 2
filt_width = 3
filt_height = 3

input_sizes_nhwc = [N, H, W, C]
filter_size_hwio = [filt_height, filt_width, I, O]
out_backprop_valid = [1, 3, 2, 2]
out_backprop_same = [1, 4, 3, 2]
out_backprop_in_sizes = {"VALID": out_backprop_valid, "SAME": out_backprop_same}
stride = [1, 2, 2, 1]


#TF graph
def tf_model(padding):
    t1 = tf.placeholder(dtype=tf.float32, shape=input_sizes_nhwc, name='t1')
    t2 = tf.constant(filter_size_hwio, dtype=tf.int32, name='t1')
    t3 = tf.placeholder(
        dtype=tf.float32, shape=out_backprop_in_sizes[padding], name='t3')

    #Cast dtype to bfloat16 for TF because NNP casts ng_model inputs
    t1 = tf.cast(t1, dtype=tf.bfloat16)
    t3 = tf.cast(t3, dtype=tf.bfloat16)

    filt = nn_ops.conv2d_backprop_filter(
        t1, t2, t3, stride, padding=padding, data_format='NHWC')

    #Cast dtype back to float32 similar to NNP
    filt = tf.cast(filt, dtype=tf.float32)
    return filt, t1, t3


#Ngraph Graph
def ng_model(padding):
    t1 = tf.placeholder(dtype=tf.float32, shape=input_sizes_nhwc, name='t1')
    t2 = tf.constant(filter_size_hwio, dtype=tf.int32, name='t1')
    t3 = tf.placeholder(
        dtype=tf.float32, shape=out_backprop_in_sizes[padding], name='t3')

    filt = nn_ops.conv2d_backprop_filter(
        t1, t2, t3, stride, padding=padding, data_format='NHWC')
    return filt, t1, t3


config = tf.ConfigProto(
    allow_soft_placement=True,
    log_device_placement=False,
    inter_op_parallelism_threads=1)


@pytest.mark.parametrize("padding", ("VALID", "SAME"))
def test_conv2dbackpropfilter_nhwc(padding):
    np_inp = np.random.rand(*input_sizes_nhwc).astype('f')
    np_out = np.random.rand(*out_backprop_in_sizes[padding]).astype('f')

    with tf.Session(config=config) as sess_tf:
        ngraph_bridge.disable()
        tf_out, input_data, out_backprop = tf_model(padding)
        feed_dict = {input_data: np_inp, out_backprop: np_out}
        tf_outval = sess_tf.run(tf_out, feed_dict=feed_dict)

    #Test 2: model2 with ngraph, NNP backend
    with tf.Session(config=config) as sess_ng:
        ngraph_bridge.enable()
        ngraph_bridge.update_config(config)
        os.environ['NGRAPH_TF_DISABLE_DEASSIGN_CLUSTERS'] = '1'
        ng_out, input_data, out_backprop = ng_model(padding)
        feed_dict = {input_data: np_inp, out_backprop: np_out}
        ng_outval = sess_ng.run(ng_out, feed_dict=feed_dict)

    assert np.allclose(tf_outval, ng_outval, rtol=0, atol=1e-02)
