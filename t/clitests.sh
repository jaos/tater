#!/bin/bash
#
# Copyright (C) 2022-2023 Jason Woodward <woodwardj at jaos dot org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
set -fexuo pipefail

if [ -z "${1:-}" ]; then
    echo "$0 <binary>"
    exit 1
fi
tater=$1

set +feuo pipefail
echo -e "let a = 1;\nprint a; exit(77);" | ${tater} -d -s
if [ ${?} != 77 ]; then
    exit 1
fi
set -feuo pipefail

${tater} /tmp/nosuchfileordir || true # expected failure
${tater} /tmp/nosuchfileordir noarg || true # expected failure
echo -e "invalidsyntax;" | ${tater} -d -s || true

TEST_TMPDIR=$(mktemp -d)
trap "rm -rf ${TEST_TMPDIR};" err exit
echo -e "let a = 1;\nprint a;" > "${TEST_TMPDIR}/t.tot"
${tater} -d -s "${TEST_TMPDIR}/t.tot"
echo -e "garbage" >> "${TEST_TMPDIR}/garbage.tot"
${tater} -d -s "${TEST_TMPDIR}/garbage.tot" || true
${tater} -v
${tater} -h
${tater} -d -s -invalid-option-will-fail || true
