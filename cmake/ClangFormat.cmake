# Formatting of the changed lines only, the same way on macOS, Linux and Windows (no shell, no
# Python: just CMake, git and clang-format).
#
# Included from CMakeLists.txt, it defines two optional targets (never part of ALL or CTest):
#   cmake --build --preset debug --target format-check   # fails if a changed line is off-style
#   cmake --build --preset debug --target format         # rewrites only the changed lines
# Run as a script, it does the work:
#   cmake -DMODE=check|fix -DSOURCE_DIR=<repo> -DCLANG_FORMAT=<exe> -DGIT=<exe> [-DBASE=HEAD]
#         -P cmake/ClangFormat.cmake
#
# "Changed" = staged + unstaged changes against BASE (default HEAD) and untracked C++ files,
# which count as wholly new. Lines that only lost text are skipped (nothing left to format).

# Major used to calibrate .clang-format; keep it equal to the comment at the top of that file.
set(POINTCLICK_CLANG_FORMAT_MAJOR 21)
set(POINTCLICK_FORMAT_INSTALL_HINT
        "pip install \"clang-format==${POINTCLICK_CLANG_FORMAT_MAJOR}.*\" (macOS, Linux, Windows)")

if(NOT CMAKE_SCRIPT_MODE_FILE)
    # ---- Configure time: define the targets. Nothing here can fail the configure step. ----
    set(POINTCLICK_FORMAT_BASE "HEAD" CACHE STRING
            "Git revision the format/format-check targets compare against")
    if(NOT POINTCLICK_CLANG_FORMAT)
        set(_pointclick_clang_format_hints "")
        if(APPLE)
            execute_process(COMMAND xcrun --find clang-format
                    OUTPUT_VARIABLE _pointclick_xcrun_clang_format
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _pointclick_xcrun)
            if(_pointclick_xcrun EQUAL 0 AND _pointclick_xcrun_clang_format)
                get_filename_component(_pointclick_clang_format_hints
                        "${_pointclick_xcrun_clang_format}" DIRECTORY)
            endif()
        endif()
    endif()
    # Versioned names first (Linux distros), then the plain one (pip, Homebrew, LLVM for Windows).
    find_program(POINTCLICK_CLANG_FORMAT
            NAMES clang-format-${POINTCLICK_CLANG_FORMAT_MAJOR} clang-format
            HINTS ${_pointclick_clang_format_hints}
            DOC "clang-format used by the format/format-check targets")
    find_package(Git QUIET)

    set(_pointclick_format_args
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DCLANG_FORMAT=${POINTCLICK_CLANG_FORMAT}
            -DGIT=${GIT_EXECUTABLE}
            -DBASE=${POINTCLICK_FORMAT_BASE}
            -P ${CMAKE_CURRENT_LIST_FILE})
    add_custom_target(format-check
            COMMAND ${CMAKE_COMMAND} -DMODE=check ${_pointclick_format_args}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "clang-format: checking the lines changed since ${POINTCLICK_FORMAT_BASE}"
            VERBATIM USES_TERMINAL)
    add_custom_target(format
            COMMAND ${CMAKE_COMMAND} -DMODE=fix ${_pointclick_format_args}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "clang-format: formatting the lines changed since ${POINTCLICK_FORMAT_BASE}"
            VERBATIM USES_TERMINAL)
    if(NOT POINTCLICK_CLANG_FORMAT)
        message(STATUS "clang-format not found: format/format-check will explain how to get it")
    endif()
    return()
endif()

# ---- Script mode: format or check the changed lines. ----
if(NOT MODE MATCHES "^(check|fix)$")
    message(FATAL_ERROR "MODE must be check or fix")
endif()
if(NOT CLANG_FORMAT OR NOT EXISTS "${CLANG_FORMAT}")
    message(FATAL_ERROR "clang-format not found. Install ${POINTCLICK_FORMAT_INSTALL_HINT}, "
            "then reconfigure (or pass -DPOINTCLICK_CLANG_FORMAT=<path>).")
endif()
if(NOT GIT OR NOT EXISTS "${GIT}")
    message(FATAL_ERROR "git not found: format/format-check need it to find the changed lines.")
endif()
if(NOT BASE)
    set(BASE "HEAD")
endif()

execute_process(COMMAND "${CLANG_FORMAT}" --version
        OUTPUT_VARIABLE version_text OUTPUT_STRIP_TRAILING_WHITESPACE)
if(version_text MATCHES "version ([0-9]+)\\.")
    set(found_major "${CMAKE_MATCH_1}")
else()
    set(found_major "?")
endif()
if(NOT found_major STREQUAL POINTCLICK_CLANG_FORMAT_MAJOR)
    message(WARNING "${CLANG_FORMAT} is \"${version_text}\", but .clang-format is calibrated for "
            "clang-format ${POINTCLICK_CLANG_FORMAT_MAJOR}; the result may differ from other "
            "contributors'. Install ${POINTCLICK_FORMAT_INSTALL_HINT}.")
endif()

set(patterns "*.cpp" "*.hpp" "*.h")
execute_process(COMMAND "${GIT}" diff --name-only --diff-filter=ACMR "${BASE}" -- ${patterns}
        WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE changed_text RESULT_VARIABLE git_result
        ERROR_VARIABLE git_error)
if(NOT git_result EQUAL 0)
    message(FATAL_ERROR "git diff ${BASE} failed: ${git_error}")
endif()
execute_process(COMMAND "${GIT}" ls-files --others --exclude-standard -- ${patterns}
        WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE untracked_text)
string(REGEX MATCHALL "[^\n]+" changed_files "${changed_text}")
string(REGEX MATCHALL "[^\n]+" untracked_files "${untracked_text}")

set(checked 0)
set(failed "")
foreach(file IN LISTS changed_files untracked_files)
    set(line_args "")
    if(NOT file IN_LIST untracked_files)
        # A hunk header is the only diff line that starts with "@@" (content lines get a prefix).
        execute_process(COMMAND "${GIT}" diff -U0 --no-color "${BASE}" -- "${file}"
                WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE file_diff)
        string(REGEX MATCHALL "\n@@ -[0-9,]+ \\+[0-9]+(,[0-9]+)? @@" hunks "${file_diff}")
        foreach(hunk IN LISTS hunks)
            string(REGEX MATCH "\\+([0-9]+)(,([0-9]+))?" range "${hunk}")
            set(first "${CMAKE_MATCH_1}")
            set(count "${CMAKE_MATCH_3}")
            if(count STREQUAL "")
                set(count 1)
            endif()
            if(count GREATER 0)
                math(EXPR last "${first} + ${count} - 1")
                list(APPEND line_args "--lines=${first}:${last}")
            endif()
        endforeach()
        if(NOT line_args)
            continue() # only deletions
        endif()
    endif()
    math(EXPR checked "${checked} + 1")
    if(MODE STREQUAL "fix")
        execute_process(COMMAND "${CLANG_FORMAT}" --style=file -i ${line_args} "${file}"
                WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE result)
    else()
        execute_process(COMMAND "${CLANG_FORMAT}" --style=file --dry-run --Werror ${line_args}
                "${file}" WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE result)
    endif()
    if(NOT result EQUAL 0)
        list(APPEND failed "${file}")
    endif()
endforeach()

if(failed)
    list(JOIN failed "\n  " failed_text)
    if(MODE STREQUAL "fix")
        message(FATAL_ERROR "clang-format failed on:\n  ${failed_text}")
    endif()
    message(FATAL_ERROR "Changed lines out of style (see the warnings above) in:\n  "
            "${failed_text}\nFix them with: cmake --build <build dir> --target format")
endif()
if(MODE STREQUAL "fix")
    message(STATUS "clang-format: formatted the changed lines of ${checked} file(s) since ${BASE}")
else()
    message(STATUS "clang-format: ${checked} changed file(s) since ${BASE}, all in style")
endif()
