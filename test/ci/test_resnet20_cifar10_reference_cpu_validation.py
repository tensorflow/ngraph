# Test intended to be run using pytest
#
# If pytest is not installed on your server, you can install it in a virtual
# environment:
#
#   $ virtualenv -p /usr/bin/python venv-pytest
#   $ source venv-pytest/bin/activate
#   $ pip -U pytest
#   $ pytest test_resnet20_cifar10_reference_cpu_validation.py
#   $ deactivte
#
# This test has no command-line parameters, as it is run via pytest.
# This test does have environment variables that can alter how the run happens:
#
#     Parameter              Purpose & Default (if any)
#
#     TEST_RESNET20_CIFAR10_EPOCHS       Number of epochs (steps) to run;
#                                        default=250
#     TEST_RESNET20_CIFAR10_EVAL_EPOCHS  Number of epoch to run between each
#                                        eval; default=#-of-EPOCHS
#     TEST_RESNET20_CIFAR10_BATCH_SIZE   Batch size per step; default=128
#
#     TEST_RESNET20_CIFAR10_DATA_DIR     Directory where CIFAR10 datafiles are
#                                        located
#     TEST_RESNET20_CIFAR10_LOG_DIR      Optional: dir to write log files to
#
# JUnit XML files can be generated using pytest's command-line options.
# For example:
# $ pytest -s ./test_resnet20_cifar10_cpu_daily_validation.py --junit-xml=../validation_tests_resnet20_cifar10_cpu.xml --junit-prefix=daily_validation_resnet20_cifar10_cpu
#
# Example of ngraph command-line used:
# $ KMP_BLOCKTIME=1 OMP_NUM_THREADS=56 KMP_AFFINITY=granularity=fine,compact,1,0 python cifar10_main.py --data_dir /tmp/cifar10_input_data --train_epochs 1 --batch_size 128 --resnet_size=20 --select_device NGRAPH --model_dir /tmp/cifar10-ngraph --epochs_between_evals 1 --inter_op 2
#

import sys
import os
import re
import json
import datetime as DT

import lib_validation_testing as VT


# Constants

# Log files
kResnet20CPURefLog         = 'test_resnet20_cifar10_cpu_reference.log'
kResnet20CPURefJson        = 'test_resnet20_cifar10_cpu_reference.json'
kResnet20RefSummaryLog     = 'test_resnet20_cifar10_cpu_reference_summary.log'
kResnet20JenkinsRefSummLog = 'test_resnet20_cifar10_cpu_jenkins_ref_oneline.log'

# Default number of epochs (steps) is 250
kDefaultEpochs = 250

# As per Alex Krizhevsky's description of the CIFAR-10 dataset at:
#   https://www.cs.toronto.edu/~kriz/cifar.html
kTrainEpochSize = 50000  

# Batch size is set with --data_dir.  For now, it is a constant
kTrainBatchSize = 128

# Relative path (from top of bridge repo) to cifar10_main.py script
kResnet20ScriptPath = 'test/resnet/cifar10_main.py'

# Python program to run script.  This should just be "python" (or "python2"),
# as the virtualenv relies on PATH resolution to find the python executable
# in the virtual environment's bin directory.
kPythonProg = 'python'

# Date and time that testing started, for the run stats collected later
kRunDateTime = DT.datetime.now()


def test_resnet20_cifar10_ngraph_cpu_backend():

    # This *must* be run inside the test, because env. var. PYTEST_CURRENT_TEST
    # only exists when inside the test function.
    ngtfDir = VT.findBridgeRepoDirectory()
    script = os.path.join(ngtfDir, kResnet20ScriptPath)
    VT.checkScript(script)

    dataDir = os.environ.get('TEST_RESNET20_CIFAR10_DATA_DIR', None)
    if not os.path.isdir(dataDir):
        raise Exception('Directory %s does not exist' % str(dataDir))

    epochs = int(os.environ.get('TEST_RESNET20_CIFAR10_EPOCHS', kDefaultEpochs))

    # Run with CPU backend, saving timing and accuracy
    refLog = VT.runResnetScript(logID=' Reference',
                                useNGraph=False,
                                script=script,
                                python=kPythonProg,
                                epochs=epochs,
                                batchSize=kTrainBatchSize,
                                dataDirectory=dataDir,
                                verbose=False)  # log-device-placement
    refResults = \
        VT.collect_resnet20_cifar10_results(runType='Reference CPU backend',
                                            log=refLog,
                                            date=str(kRunDateTime),
                                            epochs=epochs,
                                            batchSize=kTrainBatchSize)

    print
    print('Collected results for *reference* run are:')
    print(json.dumps(refResults, indent=4))
    
    lDir = None
    if os.environ.has_key('TEST_RESNET20_CIFAR10_LOG_DIR'):
        lDir = os.path.abspath(os.environ['TEST_RESNET20_CIFAR10_LOG_DIR'])
        # Dump logs to files, for inclusion in Jenkins artifacts
        VT.writeLogToFile(refLog,
                          os.path.join(lDir, kResnet20CPURefLog))
        VT.writeJsonToFile(json.dumps(refResults, indent=4),
                           os.path.join(lDir, kResnet20CPURefJson))

    print
    print '----- RESNET20 CIFAR10 ReferenceTesting Summary -------------------------------'

    summaryLog = None
    if lDir != None:
        summaryLog = os.path.join(lDir, kResnet20RefSummaryLog)

    logOut = VT.LogAndOutput(logFile=summaryLog)

    # Report commands
    logOut.line()
    logOut.line('Reference run with CPU: %s' % refResults['command'])

    # Report parameters
    logOut.line()
    logOut.line('Epochs:                   %d' % epochs)
    logOut.line('Batch size:               %d' % kTrainBatchSize)
    logOut.line('Epoch size:               %d (fixed in CIFAR10)'
                % kTrainEpochSize)
    logOut.line('Reference back-end used:  %s' % 'CPU')
    logOut.line('Data directory:           %s' % dataDir)

    refAccuracy = float(refResults['accuracy'])

    # Report accuracy
    logOut.line()
    logOut.line('Accuracy, in reference run using CPU:  %7.6f' % refAccuracy)

    # Report on times
    logOut.line()
    logOut.line('Reference run using CPU took: %f seconds'
                % refResults['wallclock'])

    # Make sure all output has been flushed before running assertions
    logOut.flush()

# End: test_resnet20_cifar10_cpu_backend()
