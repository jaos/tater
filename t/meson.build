
test_sources = [
  'test.c',
]

test_path = join_paths(meson.project_source_root(), 't')

check = dependency('check', required: false)
if check.found()
  testapp = executable('testapp', test_sources, link_with: [libtatertot], dependencies: check, install: false)
  test('testsuite', testapp, is_parallel: true, workdir: test_path, env: [], timeout: 30)
endif
test('clitest', find_program('clitests.sh'), args: [tater.full_path()], depends: [tater])
