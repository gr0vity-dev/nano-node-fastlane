#!/bin/bash
set -euo pipefail

qt_dir=${1}
build_target=${2:-all}

OS=$(uname)

source "$(dirname "$BASH_SOURCE")/impl/code-inspector.sh"
code_inspect "${ROOTPATH:-.}"

mkdir -p build
pushd build

if [[ "${RELEASE:-false}" == "true" ]]; then
    BUILD_TYPE="RelWithDebInfo"
fi

if [[ ${ASAN_INT:-0} -eq 1 ]]; then
    SANITIZER="ASAN_INT"
elif [[ ${ASAN:-0} -eq 1 ]]; then
    SANITIZER="ASAN"
elif [[ ${TSAN:-0} -eq 1 ]]; then
    SANITIZER="TSAN"
elif [[ ${LCOV:-0} -eq 1 ]]; then
    COVERAGE="ON"
fi

ulimit -S -n 8192

if [[ "$OS" == 'Linux' ]]; then
    if clang --version && [ ${LCOV:-0} == 0 ]; then
        COMPILER="clang"
    fi
fi

NANO_NETWORK="dev"
NANO_TEST="ON"
NANO_GUI="ON"
QT_DIR=${qt_dir}
export BUILD_TYPE NANO_NETWORK NANO_TEST NANO_GUI COVERAGE QT_DIR COMPILER SANITIZER

$(dirname "$BASH_SOURCE")/build.sh ${build_target}

popd