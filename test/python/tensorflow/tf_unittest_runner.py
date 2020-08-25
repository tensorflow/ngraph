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
import unittest
import sys
import argparse
import os
import re
import fnmatch
import time
from datetime import timedelta
import warnings
import multiprocessing

try:
    import xmlrunner
except:
    os.system('pip install unittest-xml-reporting')
    import xmlrunner
os.environ['NGRAPH_TF_DISABLE_DEASSIGN_CLUSTERS'] = '1'
"""
tf_unittest_runner is primarily used to run tensorflow python 
unit tests using ngraph
"""


def main():
    parser = argparse.ArgumentParser()
    optional = parser._action_groups.pop()
    required = parser.add_argument_group('required arguments')

    required.add_argument(
        '--tensorflow_path',
        help=
        "Specify the path to Tensorflow source code. Eg:ngraph-bridge/build_cmake/tensorflow \n",
        required=True)
    optional.add_argument(
        '--list_tests',
        help=
        "Prints the list of test cases in this package. Eg:math_ops_test.* \n")
    optional.add_argument(
        '--run_test',
        help=
        "Runs the testcase and returns the output. Eg:math_ops_test.DivNoNanTest.testBasic"
    )
    optional.add_argument(
        '--run_tests_from_file',
        help="""Reads the test names specified in a file and runs them. 
        Eg:--run_tests_from_file=tests_to_run.txt""")
    optional.add_argument(
        '--xml_report',
        help=
        "Generates results in xml file for jenkins to populate in the test result \n"
    )
    optional.add_argument(
        '--verbose',
        action="store_true",
        help="Prints standard out if specified \n")
    parser._action_groups.append(optional)
    arguments = parser.parse_args()

    xml_report = arguments.xml_report

    if (arguments.list_tests):
        test_list = get_test_list(arguments.tensorflow_path,
                                  arguments.list_tests)
        skip_tests_from_list(test_list[0])
        print('\n'.join(test_list[0]))
        return None, None

    if (arguments.run_test):
        invalid_list = []
        start = time.time()
        test_list = get_test_list(arguments.tensorflow_path, arguments.run_test)
        for test in test_list[1]:
            if test is not None:
                invalid_list.append(test_list[1])
                result_str = "\033[91m INVALID \033[0m " + test + \
                '\033[91m' + '\033[0m'
                print('TEST:', result_str)
        skip_tests_from_list(test_list[0])
        test_results = run_test(test_list[0], xml_report,
                                (2 if arguments.verbose else 0))
        elapsed = time.time() - start
        print("\n\nTesting results\nTime elapsed: ",
              str(timedelta(seconds=elapsed)))
        return check_and_print_summary(test_results, test_list[1])

    if (arguments.run_tests_from_file):
        all_test_list = []
        invalid_list = []
        start = time.time()
        list_of_tests = read_tests_from_file(arguments.run_tests_from_file)
        list_of_tests = list(dict.fromkeys(list_of_tests))  # remove dups
        skip_tests_from_list(list_of_tests)
        for test in list_of_tests:
            test_list = get_test_list(arguments.tensorflow_path, test)
            for test in test_list[1]:
                if test is not None:
                    invalid_list.append(test_list[1])
                    result_str = "\033[91m INVALID \033[0m " + test + \
                    '\033[91m' + '\033[0m'
                    print('TEST:', result_str)
            test_list = list(set(test_list[0]))
            for test_name in test_list:
                if test_name not in all_test_list:
                    all_test_list.append(test_name)
        test_results = run_test(all_test_list, xml_report,
                                (2 if arguments.verbose else 0))
        elapsed = time.time() - start
        print("\n\nTesting results\nTime elapsed: ",
              str(timedelta(seconds=elapsed)))
        return check_and_print_summary(test_results, invalid_list)


def get_test_list(tf_path, test_regex):
    accepted_formats = [
        "math_ops_test.DivNoNanTest.testBasic", "math_ops_test.DivNoNanTest.*",
        "math_ops_test.D*", "math_ops_test.*", "math_*_test", "math_*_*_test",
        "math*_test"
    ]
    try:
        module_list = regex_walk(tf_path, test_regex)
    except Exception as e:
        module_list = []
        print(
            "Exception occured in regex_walk. " + str(e) +
            """\nInvalid module name. Use bazel query below to get list of tensorflow python test modules.
            bazel query 'kind(".*_test rule", //tensorflow/python:nn_test)' --output label\n"""
        )
    try:
        test_list = list_tests(module_list, test_regex)
    except Exception as e:
        test_list = [[], []]
        print(
            "Exception occured in list_tests. " + str(e) +
            "\nEnter a valid argument to --list_tests or --run_test.\n \nLIST OF ACCEPTED FORMATS:"
        )
        print('\n'.join(accepted_formats))
    return test_list


from fnmatch import fnmatch


def regex_walk(dirname, regex_input):
    """
    Adds all the directories under the specified dirname to the system path to 
    be able to import the modules.
    
    Args:
    dirname: This is the tensorflow_path passed as an argument is the path to 
    tensorflow source code.
    
    regex_input: Regular expression input string to filter and list/run tests.
    Few examples of accepted regex_input are:
    math_ops_test.DivNoNanTest.testBasic
    math_ops_test.DivNoNanTest.*
    math_ops_test.D*
    math_ops_test.*
    math_*_test
    math_*_*_test
    math*_test
    """
    if (re.search("\.", regex_input) is None):
        test = regex_input + '.py'
    else:
        test = (re.split("\.", regex_input))[0] + '.py'
    module_list = []
    for path, subdirs, files in os.walk(dirname):
        for name in files:
            if fnmatch(name, test):
                sys.path.append(path)
                name = os.path.splitext(name)[0]
                module_list.append(name)
    if not module_list:
        print("Test name does not exist")
        sys.exit(1)
    return module_list


def list_tests(module_list, regex_input):
    """
    Generates a list of test suites and test cases from a TF test target 
    specified. 

    Args:
    module_list: This is a list tensorflow test target names passed as an argument.
    Example --list_tests=math_ops_test.R*
    To get the list of tensorflow python test modules, query using bazel.
    bazel query 'kind(".*_test rule", //tensorflow/python/...)' --output label

    regex_input: Regular expression input strings to filter and list tests. 
    Few examples of accepted regex_input are:
    math_ops_test.DivNoNanTest.testBasic
    math_ops_test.DivNoNanTest.*
    math_ops_test.D*
    math_ops_test.*
    math_*_test
    math_*_*_test
    math*_test
    """
    loader = unittest.TestLoader()
    alltests = []
    listtests = []
    invalidtests = []
    for test_module in module_list:
        module = __import__(test_module)
        if (module is None):
            print("Enter a valid test name to run")
        test_modules = loader.loadTestsFromModule(module)
        for test_class in test_modules:
            for i in test_class:
                alltests.append(i.id())

    for test in alltests:
        if test == regex_input:
            listtests.append(test)
        elif (re.search("\*", regex_input)):
            test_name = (re.split("\*", regex_input))[0]
            if test_name in test:
                listtests.append(test)

    if not listtests:
        invalidtests.append(regex_input)

    return listtests, invalidtests


def read_tests_from_file(filename):
    with open(filename) as list_of_tests:
        return [
            line.split('#')[0].rstrip('\n').strip(' ')
            for line in list_of_tests.readlines()
            if line[0] != '#' and line != '\n'
        ]


def skip_tests_from_list(orig_list_of_tests):
    if ('TFPYTEST_SKIPLIST' in os.environ):
        filename = os.path.abspath(os.environ['TFPYTEST_SKIPLIST'])
        with open(filename) as skipfile:
            orig_count = len(orig_list_of_tests)
            print()
            for line in skipfile.readlines():
                line = line.split('#')[0].rstrip('\n').strip(' ')
                if line == '':
                    continue
                if line in orig_list_of_tests:
                    print('will skip test:', line)
                    orig_list_of_tests.remove(line)
            new_count = len(orig_list_of_tests)
            print('\nTest count: Initial={} Skipped={} New-Total={}\n'.format(
                orig_count, orig_count - new_count, new_count))
            #return orig_list_of_tests


def func_utrunner_testcase_run(return_dict, runner, aTest):
    test_result = runner.run(aTest)
    return_dict[aTest.id()] = {
        'wasSuccessful': test_result.wasSuccessful(),
        'failures': test_result.failures,
        'errors': test_result.errors,
        'skipped': test_result.skipped
    }


def run_singletest_in_new_child_process(runner, aTest):
    manager = multiprocessing.Manager()
    return_dict = manager.dict()
    p = multiprocessing.Process(
        target=func_utrunner_testcase_run, args=(return_dict, runner, aTest))
    p.start()
    p.join()

    #  A negative exitcode -N indicates that the child was terminated by signal N.
    if p.exitcode != 0:
        error_msg = '!!! RUNTIME ERROR !!! Test ' + aTest.id(
        ) + ' exited with code: ' + str(p.exitcode)
        print(error_msg)
        return_dict[aTest.id()] = {
            'wasSuccessful': False,
            'failures': [('', error_msg)],
            'errors': [('', error_msg)],
            'skipped': []
        }
        return return_dict[aTest.id()]

    test_result_map = return_dict[aTest.id()]
    return test_result_map


def run_test(test_list, xml_report, verbosity=0):
    """
    Runs a specific test suite or test case given with the fully qualified 
    test name and prints stdout.

    Args:
    test_list: This is the list of tests to run,filtered based on the 
    regex_input passed as an argument.
    Example: --run_test=math_ops_test.A*   
    verbosity: Python verbose logging is set to 2. You get the help string 
    of every test and the result.
    """
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    succeeded = []
    failures = []
    skipped = []
    if xml_report is not None:
        for testpattern in test_list:
            tests = loader.loadTestsFromName(testpattern)
            suite.addTest(tests)
        with open(xml_report, 'wb') as output:
            sys.stdout = open(os.devnull, "w")
            sys.stderr = open(os.devnull, "w")
            test_result = xmlrunner.XMLTestRunner(
                output=output, verbosity=verbosity).run(suite)
            sys.stderr = sys.__stderr__
            sys.stdout = sys.__stdout__
            failures.extend(test_result.failures)
            failures.extend(test_result.errors)
            succeeded.extend(test_result.successes)

        summary = {"TOTAL": test_list, "PASSED": succeeded, "FAILED": failures}
        return summary
    else:
        runner = unittest.TextTestRunner(verbosity=verbosity)
        for testpattern in test_list:
            tests = loader.loadTestsFromName(testpattern)
            for aTest in tests:
                print()
                print('>> >> >> >> Testing:', aTest.id(), '...')
                start = time.time()
                test_result_map = run_singletest_in_new_child_process(
                    runner, aTest)
                elapsed = time.time() - start
                elapsed = str(timedelta(seconds=elapsed))

                if test_result_map['wasSuccessful'] == True:
                    succeeded.append(aTest.id())
                    result_str = " \033[92m OK \033[0m " + aTest.id()
                elif 'failures' in test_result_map and bool(
                        test_result_map['failures']):
                    failures.append(test_result_map['failures'])
                    print('test_result_map[\'failures\']',
                          test_result_map['failures'])
                    result_str = " \033[91m FAIL \033[0m " + aTest.id() + \
                        '\n\033[91m' + ''.join(test_result_map['failures'][0][1]) + '\033[0m'
                elif 'errors' in test_result_map and bool(
                        test_result_map['errors']):
                    failures.append(test_result_map['errors'])
                    result_str = " \033[91m FAIL \033[0m " + aTest.id() + \
                        '\n\033[91m' + ''.join(test_result_map['errors'][0][1]) + '\033[0m'
                skipped.append(test_result_map['skipped'])
                print('took', elapsed, 'RESULT =>', result_str)
        summary = {
            "TOTAL": test_list,
            "PASSED": succeeded,
            "FAILED": failures,
        }
        return summary


def check_and_print_summary(test_results, invalid_list):
    print('========================================================')
    print("TOTAL: ", len(test_results['TOTAL']))
    print("PASSED: ", len(test_results['PASSED']))
    print("FAILED: ", len(test_results['FAILED']))

    if (len(invalid_list) > 0):
        print("INVALID: ", len(invalid_list))

    print('========================================================\n')

    if len(test_results['FAILED']) == 0:
        return True
    else:
        return False


if __name__ == '__main__':
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        status = main()
        if status == False:
            raise Exception("Tests failed")
