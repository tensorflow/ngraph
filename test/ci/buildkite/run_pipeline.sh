#!/bin/bash

set -euo pipefail

echo "BUILDKITE_AGENT_META_DATA_QUEUE: ${BUILDKITE_AGENT_META_DATA_QUEUE}"
echo "BUILDKITE_AGENT_META_DATA_NAME: ${BUILDKITE_AGENT_META_DATA_NAME}"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Always run setup for now
PIPELINE_STEPS=" ${SCRIPT_DIR}/setup.yml "
if [ "${BUILDKITE_PIPELINE_NAME}" == "cpu-grappler" ]; then
   export BUILD_OPTIONS=--use_grappler
   export TF_LOCATION=/localdisk/buildkite-agent/prebuilt_tensorflow_2_2_0
   PIPELINE_STEPS+=" ${SCRIPT_DIR}/cpu.yml "
elif [ "${BUILDKITE_PIPELINE_NAME}" == "cpu" ]; then
   export NGRAPH_TF_BACKEND=CPU
   export TF_LOCATION=/localdisk/buildkite-agent/prebuilt_tensorflow_2_2_0
   PIPELINE_STEPS+=" ${SCRIPT_DIR}/cpu.yml "
elif [ "${BUILDKITE_PIPELINE_NAME}" == "cpu-intel-tf" ]; then
   export BUILD_OPTIONS=--use_intel_tensorflow
   export TF_LOCATION=/localdisk/buildkite-agent/prebuilt_intel_tensorflow
   export NGRAPH_TF_BACKEND=CPU
   PIPELINE_STEPS+=" ${SCRIPT_DIR}/cpu.yml "
elif [ "${BUILDKITE_PIPELINE_NAME}" == "igpu" ]; then
   export NGRAPH_TF_BACKEND=GPU
   export TF_LOCATION=/localdisk/buildkite-agent/prebuilt_tensorflow_2_2_0
   PIPELINE_STEPS+=" ${SCRIPT_DIR}/cpu.yml "
else
   export TF_LOCATION=/localdisk/buildkite-agent/prebuilt_tensorflow_2_2_0
   PIPELINE_STEPS+=" ${SCRIPT_DIR}/${BUILDKITE_PIPELINE_NAME}.yml "
fi

cat ${PIPELINE_STEPS} | buildkite-agent pipeline upload
