new file mode 100755
#!/bin/bash

export QT_DIR=${1}
export NANO_GUI="ON"
export PORTABLE="1"
export CI_VERSION_PRE_RELEASE=${CI_VERSION_PRE_RELEASE:-"OFF"}
export COMPILER=${COMPILER:-"gcc"}

set -o errexit
set -o nounset
set -o xtrace
OS=$(uname)

if [[ "${NETWORK}" == "BETA" ]]; then
    export NANO_NETWORK="beta"
    export BUILD_TYPE="RelWithDebInfo"
elif [[ "${NETWORK}" == "TEST" ]]; then
    export NANO_NETWORK="test"
    export BUILD_TYPE="RelWithDebInfo"
else
    export NANO_NETWORK="live"
    export BUILD_TYPE="Release"
fi

$(dirname "$BASH_SOURCE")/build.sh package

