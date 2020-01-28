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
"""nGraph TensorFlow axpy_variable_update

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import getpass
import ctypes

import warnings
warnings.filterwarnings('ignore', category=FutureWarning)

import numpy as np
import tensorflow as tf
from tensorflow.python.client import timeline
import json

import os
os.environ['NGRAPH_TF_USE_DEVICE_MODE'] = "1"
import ngraph_bridge

print("TensorFlow version: ", tf.VERSION)


@tf.function
def run():
    # Setup TensorBoard
    graph_location = "/tmp/" + getpass.getuser() + "/tensorboard-logs/test"
    print('Saving graph to: %s' % graph_location)
    train_writer = tf.summary.FileWriter(graph_location)

    # Define the data
    a = tf.constant(np.full((4, 4), 1.5, dtype=np.float32), name='alpha')
    x = tf.get_variable('x', [4, 4], initializer=tf.zeros_initializer)
    y = tf.constant(np.full((4, 4), 1.0, dtype=np.float32), name='y')

    c = a * x
    axpy = c + y

    train_step = x.assign(axpy)
    with tf.control_dependencies([train_step]):
        train_op = tf.no_op('train_op')

    # Configure the session
    config = tf.ConfigProto(
        allow_soft_placement=True,
        log_device_placement=False,
        inter_op_parallelism_threads=1,
        graph_options=tf.GraphOptions(
            optimizer_options=tf.OptimizerOptions(
                opt_level=tf.OptimizerOptions.L0,
                do_common_subexpression_elimination=False,
                do_constant_folding=False,
                do_function_inlining=False,
            )))
    #config_ngraph_enabled = ngraph_bridge.update_config(config)
    config_ngraph_enabled = config

    # Create session and run
    with tf.Session(config=config_ngraph_enabled) as sess:
        print("Python: Running with Session")
        options = tf.RunOptions(trace_level=tf.RunOptions.FULL_TRACE)
        run_metadata = tf.RunMetadata()

        event_times = []
        sess.run(tf.global_variables_initializer())
        for i in range(10):
            (result_axpy) = sess.run((train_op),
                                     options=options,
                                     run_metadata=run_metadata),
            print(i)
            event_times.append(timeline.Timeline(run_metadata.step_stats))

        print("Final value: ", x.eval())
        print("Writing event trace")
        with open('tf_event_trace.json', 'w') as f:
            f.write("[\n")
            for event in event_times:
                chrome_trace = event.generate_chrome_trace_format(
                    show_dataflow=False)
                parsed_trace = json.loads(chrome_trace)
                for tr in parsed_trace['traceEvents']:
                    f.write(json.dumps(tr) + ',\n')

    train_writer.add_graph(tf.get_default_graph())


run()
