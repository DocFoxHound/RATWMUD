# econ_watch's stamp (Docs/Design/64-economy-migration.md, Phase 13): the git state of SOURCE as this build sees it,
# written to OUT as RATW_BUILT_* (Tests/econ_watch.cpp prints it at the top of each run). Run at every build
# (CMakeLists.txt's econ_watch_stamp); OUT is rewritten only when the state changed, so econ_watch.cpp recompiles only
# then. A dirty tree's describe carries its changes' SHA-1 (`git diff HEAD`), so two builds from different uncommitted
# changes don't read alike. RATW_BUILT_CODE is the SHA-1 of the changes from the commit to the code econ_watch is built
# from (RATW_BUILT_PATHS); econ_watch works the same out at run time against the commit and warns when the two differ
# (econ::sourceDrift: a docs edit, or a docs-only commit since, is no change). Not a git checkout: "(unknown)".
set(paths CMakeLists.txt Core Tests)
execute_process(COMMAND git -C "${SOURCE}" describe --always --dirty --abbrev=12
                OUTPUT_VARIABLE describe OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C "${SOURCE}" log -1 --format=%H
                OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(describe STREQUAL "")
  set(describe "(unknown)")
endif()
if(commit STREQUAL "")
  set(commit "(unknown)")
endif()
if(describe MATCHES "-dirty$")
  execute_process(COMMAND git -C "${SOURCE}" diff --no-color --no-ext-diff --full-index HEAD OUTPUT_VARIABLE changes ERROR_QUIET)
  string(SHA1 changed "${changes}")
  string(SUBSTRING "${changed}" 0 12 changed)
  set(describe "${describe} ${changed}")
endif()
set(code "(unknown)")
if(NOT commit STREQUAL "(unknown)")
  execute_process(COMMAND git -C "${SOURCE}" diff --no-color --no-ext-diff --full-index ${commit} -- ${paths}
                  OUTPUT_VARIABLE code_changes RESULT_VARIABLE code_failed ERROR_QUIET)
  if(code_failed EQUAL 0)
    string(SHA1 code "${code_changes}")
    string(SUBSTRING "${code}" 0 12 code)
  endif()
endif()
string(REPLACE ";" " " paths_words "${paths}")
set(text "// Made by Tests/econ_watch_stamp.cmake at build time: the git state econ_watch was built from.\n"
         "#define RATW_BUILT_SOURCE \"${SOURCE}\"\n"
         "#define RATW_BUILT_DESCRIBE \"${describe}\"\n"
         "#define RATW_BUILT_COMMIT \"${commit}\"\n"
         "#define RATW_BUILT_CODE \"${code}\"\n"
         "#define RATW_BUILT_PATHS \"${paths_words}\"\n")
string(CONCAT text ${text})
set(old "")
if(EXISTS "${OUT}")
  file(READ "${OUT}" old)
endif()
if(NOT old STREQUAL text)
  file(WRITE "${OUT}" "${text}")
endif()
