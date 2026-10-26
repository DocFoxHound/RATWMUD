# econ_watch's stamp (Docs/Design/64-economy-migration.md, Phase 13): the git state of SOURCE as this build sees it,
# written to OUT as RATW_BUILT_* (Tests/econ_watch.cpp prints it at the top of each run). Run at every build
# (CMakeLists.txt's econ_watch_stamp); OUT is rewritten only when the state changed, so econ_watch.cpp recompiles only
# then. A dirty tree's describe carries its changes' SHA-1 (`git diff HEAD`), so two builds from different uncommitted
# changes don't read alike; econ_watch works the same out at run time and warns when the two differ.
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
  execute_process(COMMAND git -C "${SOURCE}" diff HEAD OUTPUT_VARIABLE changes ERROR_QUIET)
  string(SHA1 changed "${changes}")
  string(SUBSTRING "${changed}" 0 12 changed)
  set(describe "${describe} ${changed}")
endif()
set(text "// Made by Tests/econ_watch_stamp.cmake at build time: the git state econ_watch was built from.\n"
         "#define RATW_BUILT_SOURCE \"${SOURCE}\"\n"
         "#define RATW_BUILT_DESCRIBE \"${describe}\"\n"
         "#define RATW_BUILT_COMMIT \"${commit}\"\n")
string(CONCAT text ${text})
set(old "")
if(EXISTS "${OUT}")
  file(READ "${OUT}" old)
endif()
if(NOT old STREQUAL text)
  file(WRITE "${OUT}" "${text}")
endif()
