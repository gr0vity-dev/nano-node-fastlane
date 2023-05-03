#!/bin/bash
set -euo pipefail

BUILD_DIR=${1-${PWD}}

# Alpine doesn't offer an xvfb
xvfb_run_()
{
    INIT_DELAY_SEC=3

    Xvfb :2 -screen 0 1024x768x24 &
    xvfb_pid=$!
    sleep ${INIT_DELAY_SEC}
    DISPLAY=:2 $@
    res=${?}
    kill ${xvfb_pid}

    return ${res}
}

xvfb_run_ ${BUILD_DIR}/qt_test