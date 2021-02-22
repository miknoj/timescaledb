#!/usr/bin/env bash

set -e
set -o pipefail

SCRIPT_DIR=$(dirname $0)
echo $SCRIPT_DIR

TAGS="1.7.0-pg12 1.7.1-pg12 1.7.2-pg12 1.7.3-pg12 1.7.4-pg12 1.7.5-pg12 2.0.0-rc1-pg12 2.0.0-rc2-pg12 2.0.0-rc3-pg12 2.0.0-rc4-pg12 2.0.0-pg12 2.0.1-pg12 2.0.2-pg12 2.1.0-pg12"
TEST_VERSION="v6"

TAGS=$TAGS TEST_VERSION=$TEST_VERSION bash ${SCRIPT_DIR}/test_updates.sh "$@"
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
  exit $EXIT_CODE
fi
