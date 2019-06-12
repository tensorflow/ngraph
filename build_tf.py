#!/usr/bin/env python3
# ==============================================================================
#  Copyright 2018-2019 Intel Corporation
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

from tools.build_utils import *
import os, shutil
import argparse

def main():
    # Command line parser options
    parser = argparse.ArgumentParser(formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
            '--tf_version',
            type=str,
            help=
            "TensorFlow tag/branch/SHA\n",
            action="store",
            required=True)
    parser.add_argument(
            '--output_dir',
            type=str,
            help="Location where TensorFlow build will happen\n",
            action="store",
            required=True)s
    parser.add_argument(
        '--target_arch',
        help=
        "Architecture flag to use (e.g., haswell, core-avx2 etc. Default \'native\'\n",
        default="native"
    )
    arguments = parser.parse_args()

    assert not os.path.isdir(arguments.output_dir), arguments.output_dir + " already exists"
    os.mkdir(arguments.output_dir)
    os.chdir(arguments.output_dir)
    assert not is_venv(), "Please deactivate virtual environment before running this script"

    venv_dir = './venv3/'

    install_virtual_env(venv_dir)
    load_venv(venv_dir)
    setup_venv(venv_dir)

    # Download TensorFlow
    download_repo("tensorflow",
                    "https://github.com/tensorflow/tensorflow.git",
                    arguments.tf_version)

    # Build TensorFlow
    build_tensorflow(venv_dir, "tensorflow", 'artifacts',
                        arguments.target_arch, False)
    shutil.copytree('./tensorflow/tensorflow/python', './artifacts/tensorflow/python')



if __name__ == '__main__':
    main()