#!/bin/bash
set -fexuo pipefail

if [ -z "${1:-}" ]; then
    echo "$0 <binary>"
    exit 1
fi

TEST_TMPDIR=$(mktemp -d)
trap "rm -rf ${TEST_TMPDIR};" err exit

tater=$1
echo 'let a = 1;' | ${tater}
${tater} /tmp/nosuchfileordir || true # expected failure
${tater} /tmp/nosuchfileordir noarg || true # expected failure
echo 'let a = 1;' > "${TEST_TMPDIR}/t.tot"
${tater} "${TEST_TMPDIR}/t.tot"
${tater} --version
${tater} --help
${tater} --debug --gc-trace --gc-stress --invalid-option-will-fail || true
