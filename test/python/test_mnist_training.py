# Copyright 2015 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================

import sys
import pytest
import tensorflow as tf
import ngraph_bridge

import numpy as np
from common import NgraphTest

# This is the path from where the script is actually run
# which is ngraph-bridge/build_cmake/test/python

sys.path.insert(0, '../../../examples/mnist')
print('Added sys path')
from mnist_deep_simplified import *


class TestMnistTraining(NgraphTest):

    @pytest.mark.parametrize(("optimizer"), ("adam", "sgd"))
    def test_mnist_training(self, optimizer):

        class mnist_training_flags:

            def __init__(self, data_dir, model_dir, training_iterations,
                         training_batch_size, validation_batch_size,
                         make_deterministic, training_optimizer):
                self.data_dir = data_dir
                self.model_dir = model_dir
                self.train_loop_count = training_iterations
                self.batch_size = training_batch_size
                self.test_image_count = validation_batch_size
                self.make_deterministic = make_deterministic
                self.optimizer = optimizer

        data_dir = '/tmp/tensorflow/mnist/input_data'
        train_loop_count = 20
        batch_size = 50
        test_image_count = None
        make_deterministic = True
        model_dir = './mnist_trained/'

        FLAGS = mnist_training_flags(data_dir, model_dir, train_loop_count,
                                     batch_size, test_image_count,
                                     make_deterministic, optimizer)

        # Run on nGraph
        ng_loss_values, ng_test_accuracy = train_mnist_cnn(FLAGS)
        ng_values = ng_loss_values + [ng_test_accuracy]
        # Reset the Graph
        tf.reset_default_graph()

        # disable ngraph-tf
        ngraph_bridge.disable()
        tf_loss_values, tf_test_accuracy = train_mnist_cnn(FLAGS)
        tf_values = tf_loss_values + [tf_test_accuracy]

        # compare values
        assert np.allclose(
            ng_values, tf_values, atol=1e-3), "Values don't match"
