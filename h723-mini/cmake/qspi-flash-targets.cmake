# Shared `ninja flash` target for pure-QSPI apps (`app_qspi/*`) on the h723-mini.
#
# Unlike flash-targets.cmake (which programs INTERNAL flash), this flashes the
# app .hex into the on-board W25Q64 at 0x90000000 using the probe-rs OCTOSPI
# flash algorithm in ../tool/qspi_map/algo/ (target_w25q64_ospi.yaml).
#
# The algorithm driver is auto-detected: if target_w25q64_ospi.yaml is missing
# or older than flash_w25q64_ospi.c, `build_algo.py` is run first to regenerate
# it, so no manual setup step is needed.
#
# Targets:
#   ninja flash          - build the .hex, (re)build the algorithm if needed,
#                          then write it to the W25Q64 with probe-rs using its
#                          own probe auto-detection
#   ninja flash-stlink   - same, forcing the attached ST-Link
#   ninja flash-dap      - same, forcing the attached CMSIS-DAP probe
#   ninja flash-jlink    - same, forcing the attached SEGGER J-Link
#   ninja flash-ulink    - explains why ULINK cannot be automated here
#   ninja probes         - list the probes probe-rs can see right now
#
# Probe families are resolved from `probe-rs list` at configure time; see
# cmake/probe-select.cmake. Per-probe overrides:
#   -DDEBUG_PROBE=VID:PID[:SERIAL]    pin one probe for every target
#   PROBE_RS_PROBE=VID:PID[:SERIAL]   same, for a single run
#
# Prerequisite (one-time per board): h723_boot must be in internal flash.
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs    -DPYTHON=/path/to/python
#   -DQSPI_ALGO_PAGE_SIZE=0x4000

include(${CMAKE_CURRENT_LIST_DIR}/probe-select.cmake)

set(QSPI_ALGO_PAGE_SIZE "0x4000" CACHE STRING
    "probe-rs page_size in the algorithm YAML (bytes per ProgramPage call)")

find_program(PYTHON NAMES python python3 py
    HINTS "$ENV{LOCALAPPDATA}/Programs/Python"
          "$ENV{USERPROFILE}/AppData/Local/Python"
    DOC "python interpreter (to build the flash algorithm YAML)")
if(PYTHON)
    h723_tool_path("${PYTHON}" PYTHON)
endif()

set(QSPI_ALGO_DIR "${CMAKE_CURRENT_LIST_DIR}/../tool/qspi_map/algo")
set(QSPI_ALGO_SRC "${QSPI_ALGO_DIR}/flash_w25q64_ospi.c")
set(QSPI_ALGO_YAML "${QSPI_ALGO_DIR}/target_w25q64_ospi.yaml")
set(BIN_HEX "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex")

if(PROBE_RS AND PYTHON)
    # (Re)generate the algorithm YAML only when missing or older than the source.
    add_custom_command(
        OUTPUT "${QSPI_ALGO_YAML}"
        COMMAND "${PYTHON}" build_algo.py flash_w25q64_ospi.c ${QSPI_ALGO_PAGE_SIZE}
        WORKING_DIRECTORY "${QSPI_ALGO_DIR}"
        DEPENDS "${QSPI_ALGO_SRC}"
        COMMENT "Generating OCTOSPI flash algorithm ${QSPI_ALGO_YAML} ...")

    # NOTE: no ANSI colour in these messages. When the build dir is configured
    # by MSYS cmake, CMake's Ninja generator writes a /bin/sh script for
    # multi-command targets and does not quote ';' - the one inside an ANSI
    # escape would split the command. Plain text works under cmd.exe and sh.

    # One target per probe family. The command lines are spelled out in full at
    # each add_custom_target() call (rather than shared through a variable) so
    # CMake keeps every argument intact, including quoted ones containing ';'.
    #
    # No --connect-under-reset here: on this ST-Link V2 with probe-rs 0.32 the
    # under-reset attach never asserts nRST (RCC_RSR shows no PINRSTF afterwards),
    # so the chip is not reset and probe-rs times out waiting for core 0 to halt
    # ("Timeout while attaching to target under reset"). A normal attach halts the
    # core, and the OCTOSPI algorithm runs from DTCM RAM, so the W25Q64 can be
    # erased/programmed while the running app lives in it.
    foreach(_fam auto stlink dap jlink)
        if(_fam STREQUAL "auto")
            set(_tgt flash)
        else()
            set(_tgt flash-${_fam})
        endif()

        h723_probe_ready(${_fam} _ready)
        if(NOT _ready)
            h723_probe_hint(${_fam} _hint)
            add_custom_target(${_tgt}
                COMMAND ${CMAKE_COMMAND} -E echo "${_hint}"
                COMMAND ${CMAKE_COMMAND} -E false
                COMMENT "${_tgt}: no ${_fam} probe")
            continue()
        endif()

        h723_probe_args(${_fam} _pargs)
        if(_pargs)
            set(_how "${_fam} probe")
        else()
            set(_how "probe-rs auto-detect")
        endif()

        add_custom_target(${_tgt}
            COMMAND ${CMAKE_COMMAND} -E echo ""      # blank line (separate from ninja's status line)
            COMMAND ${CMAKE_COMMAND} -E echo "*** h723_boot must already be in internal flash - one time per board ***"
            COMMAND ${CMAKE_COMMAND} -E echo
                    "Writing ${PROJECT_NAME}.hex to the W25Q64 via the OCTOSPI algorithm ..."
            COMMAND "${PROBE_RS}" download ${_pargs}
                        --chip-description-path "${QSPI_ALGO_YAML}"
                        --chip "STM32H723ZG-W25Q64-w25q64_ospi" --protocol swd
                        --binary-format hex --non-interactive --disable-progressbars
                        "${BIN_HEX}"
            COMMAND "${PROBE_RS}" reset ${_pargs}
                        --chip "STM32H723ZG" --protocol swd
                        --non-interactive
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "Flashing done - board reset, booting the new app."
            DEPENDS hex "${QSPI_ALGO_YAML}"
            COMMENT "Flashing ${PROJECT_NAME}.hex to the W25Q64 (probe-rs OCTOSPI algorithm) [${_how}]"
            USES_TERMINAL)
    endforeach()

    h723_add_ulink_stub(flash)
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo
                "probe-rs and/or python not found. Set -DPROBE_RS=/path/to/probe-rs and -DPYTHON=/path/to/python.")
endif()
