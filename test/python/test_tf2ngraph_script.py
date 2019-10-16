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
"""nGraph TensorFlow bridge test for tf2ngraph script for precompilation

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import pytest
import os
import numpy as np
import shutil
import tensorflow as tf
import ngraph_bridge
from subprocess import Popen, PIPE

from tools.build_utils import command_executor
from tools.tf2ngraph import convert, get_gdef, Tf2ngraphJson

from common import NgraphTest


class Testtf2ngraph(NgraphTest):

    # utility function to make sure input format and location match
    @staticmethod
    def format_and_loc_match(format, loc):
        assert format in ['pb', 'pbtxt', 'savedmodel']
        implies = lambda p, q: (not p) or (p and q)
        #if its pbtxt, file name has pbtxt AND if its savedmodel, file name does not have pbtxt
        return implies(
            format == 'pbtxt', 'pbtxt' == loc.split('.')[-1]) and implies(
                format == 'savedmodel', 'pbtxt' != loc.split('.')[-1])

    @pytest.mark.parametrize(('commandline'),
                             (True,))  #TODO: add False for functional api test
    @pytest.mark.parametrize(
        ('inp_format', 'inp_loc', 'out_node_name'),
        (
            ('pbtxt', 'sample_graph.pbtxt', 'out_node'),
            ('savedmodel', 'sample_graph', 'out_node'),
            ('pb', 'sample_graph.pb', 'out_node'),
            ('pbtxt', 'sample_graph_nodevice.pbtxt', 'out_node'),
            ('pbtxt', 'sample_graph_nodevice.pbtxt', None),
            # this test does not pass an output node
            # and tf2ngraph is supposed to fail
        ))
    @pytest.mark.parametrize(('out_format',), (
        ('pbtxt',),
        ('pb',),
        ('savedmodel',),
    ))
    @pytest.mark.parametrize(('ng_device', 'shape_hints', 'precompile'),
                             (('CPU', [], False), ('INTERPRETER', [{}], True),
                              ('INTERPRETER', [], False)))
    # In sample_graph.pbtxt, the input shape is fully specified, so we don't need to pass shape hints for precompile
    def test_command_line_api(self, inp_format, inp_loc, out_node_name,
                              out_format, commandline, ng_device, shape_hints,
                              precompile):
        # Only run this test when grappler is enabled
        if not ngraph_bridge.is_grappler_enabled():
            return
        assert Testtf2ngraph.format_and_loc_match(inp_format, inp_loc)
        out_loc = inp_loc.split('.')[0] + '_modified' + (
            '' if out_format == 'savedmodel' else ('.' + out_format))
        try:
            (shutil.rmtree, os.remove)[os.path.isfile(out_loc)](out_loc)
        except:
            pass
        conversion_successful = False
        try:
            optional_backend_params = {
                'CPU': {
                    'device_config': '0'
                },
                'INTERPRETER': {
                    'test_echo': '1'
                }
            }[ng_device]
            config_file_name = 'temp_config_file.json'
            Tf2ngraphJson.dump_json(config_file_name, optional_backend_params,
                                    shape_hints)
            if commandline:
                # In CI this test is expected to be run out of artifacts/test/python
                # out_node_str is empty if out_node_name is None.
                # Automatic output node inference display diagnostic logs
                # But the tf2ngraph call will still fail
                out_node_str = ' ' if out_node_name is None else ' --output_nodes ' + out_node_name + ' '
                try:
                    command_executor(
                        'python ../../tools/tf2ngraph.py --input_' +
                        inp_format + ' ' + inp_loc + out_node_str +
                        ' --output_' + out_format + ' ' + out_loc +
                        ' --ng_backend ' + ng_device + ' --config_file ' +
                        config_file_name + ("", " --precompile ")[precompile])
                except:
                    assert out_node_name is None, "Call to tf2ngraph should fail when no output name is provided"
            else:
                convert(inp_format, inp_loc, out_format, out_loc, ['out_node'],
                        ng_device, optional_backend_params, shape_hints,
                        precompile)
            conversion_successful = True
        finally:
            if not conversion_successful:
                try:
                    (shutil.rmtree, os.remove)[os.path.isfile(out_loc)](out_loc)
                    os.remove(config_file_name)
                except:
                    pass
        assert conversion_successful

        gdef = get_gdef(out_format, out_loc)
        (shutil.rmtree, os.remove)[os.path.isfile(out_loc)](out_loc)
        os.remove(config_file_name)

        with tf.Graph().as_default() as g:
            tf.import_graph_def(gdef, name='')
            # The graph should have exactly one encapsulate
            assert len([
                0 for i in g.get_operations() if i.type == 'NGraphEncapsulate'
            ]) == 1
            # TODO: check that the encapsulate op has correct backend and extra params attached to it
            x = self.get_tensor(g, "x:0", False)
            y = self.get_tensor(g, "y:0", False)
            out = self.get_tensor(g, "out_node:0", False)

            sess_fn = lambda sess: sess.run(
                [out], feed_dict={i: np.zeros((10,)) for i in [x, y]})

            res1 = self.with_ngraph(sess_fn)
            res2 = self.without_ngraph(sess_fn)

            exp = [0.5 * np.ones((10,))]
            # Note both run on Host (because NgraphEncapsulate can only run on host)
            assert np.isclose(res1, res2).all()
            # Comparing with expected value
            assert np.isclose(res1, exp).all()

    def test_output_node_inference_for_saved_model(self):
        # The saved model we create in this pytest
        # has input and output specified,
        # and hence tf2ngraph should be able to infer it without user input
        export_dir_saved_model = 'temp_pytest_savedmodel_orig'
        export_pbtxt = 'temp_pytest_pbtxt_orig.pbtxt'
        tf2ngraph_out_loc = 'temp_pytest_pbtxt_tf2ngraph.pbtxt'

        with tf.Session() as sess:
            x = tf.placeholder(tf.float32, [None, 784], name="input")
            W = tf.constant(np.zeros([784, 10]), tf.float32)
            b = tf.constant(np.zeros([10]), tf.float32)
            linear = tf.matmul(x, W) + b
            y = tf.nn.softmax(linear, name="output")
            tf.saved_model.simple_save(
                sess,
                export_dir_saved_model,
                inputs={"inp1": x},
                outputs={
                    "out1": y,
                    "out2": linear
                })
            tf.io.write_graph(sess.graph_def, '.', export_pbtxt)

        # When the input type is saved_model, we can use signature_def to infer outputs.
        # The following test tests the signature_def code path

        p = Popen([
            'python', '../../tools/tf2ngraph.py', '--input_savedmodel',
            export_dir_saved_model, '--output_pbtxt', tf2ngraph_out_loc
        ],
                  stdin=PIPE,
                  stdout=PIPE,
                  stderr=PIPE)
        output, err = p.communicate()
        rc = p.returncode

        # Since no output nodes were provided, we expect tf2ngraph to print out possible output nodes
        diagnostic_log = "Analysed graph for possible list of output nodes. " + \
        "Please supply one or more output node in --output_nodes\n" +\
        "output of type Softmax\nadd of type Add\n"
        assert diagnostic_log in output.decode()

        # Since no output nodes were provided, we expect the test to fail with this error
        assert 'No output node name provided in --output_nodes' in err.decode()

        # we expect the test to fail, so rc should be non zero
        assert rc != 0

        shutil.rmtree(export_dir_saved_model)

        # In case or normal pb or pbtxt or saved models without signature_def
        # we rely on our code (guess_output_nodes) to guess output nodes
        # The following test tests the guess_output_nodes code path

        p = Popen([
            'python', '../../tools/tf2ngraph.py', '--input_pbtxt', export_pbtxt,
            '--output_pbtxt', tf2ngraph_out_loc
        ],
                  stdin=PIPE,
                  stdout=PIPE,
                  stderr=PIPE)
        output, err = p.communicate()
        rc = p.returncode

        assert diagnostic_log in output.decode()
        assert 'No output node name provided in --output_nodes' in err.decode()
        assert rc != 0

        os.remove(export_pbtxt)
