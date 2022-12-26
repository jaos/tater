#!/bin/bash
set -fexuo pipefail

if [ -z "${1:-}" ]; then
    echo "$0 <binary>"
    exit 1
fi

TEST_TMPDIR=$(mktemp -d)
trap "rm -rf ${TEST_TMPDIR};" err exit

clox=$1
echo 'var a = 1;' | ${clox}
${clox} /tmp/nosuchfileordir || true # expected failure
${clox} /tmp/nosuchfileordir noarg || true # expected failure
echo 'var a = 1;' > "${TEST_TMPDIR}/t.lox"
${clox} "${TEST_TMPDIR}/t.lox"
