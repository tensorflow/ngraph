import sys
import pytest
import getpass
import tensorflow as tf
import ngraph_bridge

import numpy as np
from common import NgraphTest

# This test needs access to axpy_pipelined.py script
# present in ngraph-bridge/examples
# so this path needs to be added to python path when running this

# For eg. when running the test from ngraph-bridge/build_cmake/test/python
# you can add this path as below
#sys.path.insert(0, '../../examples')

from axpy_pipelined import *
#os.environ.clear()

class TestAxpyPipelined(NgraphTest):

    def test_axpy_pipelined(self):
        prefetch_env = "NGRAPH_TF_USE_PREFETCH"
        ngraph_backend_i = "NGRAPH_TF_BACKEND"
        env_var_map = self.store_env_variables([prefetch_env, ngraph_backend_i])
        self.set_env_variable(prefetch_env, "1")
        self.set_env_variable(ngraph_backend_i, "INTERPRETER")
        input_array, output_array, expected_output_array = run_axpy_pipeline()
        print (os.environ)
        print ("------------------")
        for i in range(1, 10):
            print("Iteration:", i, " Input: ", input_array[i - 1], " Output: ",
                  output_array[i - 1], " Expected: ",
                  expected_output_array[i - 1])
            sys.stdout.flush()
            assert np.allclose(
                output_array[i - 1], expected_output_array[i - 1],
                atol=1e-3), "Output  and expected output values don't match"
        self.unset_env_variable(prefetch_env)
        self.unset_env_variable(ngraph_backend_i)
        self.restore_env_variables(env_var_map)
        print (os.environ)
