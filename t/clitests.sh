#!/bin/bash
set -fexuo pipefail

if [ -z "${1:-}" ]; then
    echo "$0 <binary>"
    exit 1
fi

TEST_TMPDIR=$(mktemp -d)
trap "rm -rf ${TEST_TMPDIR};" err exit

clox=$1
echo 'let a = 1;' | ${clox}
${clox} /tmp/nosuchfileordir || true # expected failure
${clox} /tmp/nosuchfileordir noarg || true # expected failure
echo 'let a = 1;' > "${TEST_TMPDIR}/t.lox"
${clox} "${TEST_TMPDIR}/t.lox"
${clox} --version
${clox} --help
${clox} --debug --gc-trace --gc-stress --invalid-option-will-fail || true
