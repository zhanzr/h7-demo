# Shared `ninja flash` target for pure-QSPI (app_qspi/) apps on the h750-mini.
#
# Unlike flash-targets.cmake (which programs INTERNAL flash), this flashes the
# app .hex into the on-board W25Q64 at 0x90000000 using the probe-rs QUADSPI
# flash algorithm in ../tool/qspi_map/algo/ (target_w25q64_qspi.yaml).
#
# The algorithm driver is auto-detected: if target_w25q64_qspi.yaml is missing
# or older than flash_w25q64_qspi.c, `build_algo.py` is run first to regenerate
# it, so no manual setup step is needed.
#
# Target:
#   ninja flash - build the .hex, (re)build the algorithm if needed, then write
#                 it to the W25Q64 via probe-rs (probe auto-detected, SWD)
#
# Prerequisite (one-time per board): h750_boot must be in internal flash.
# The probe is resolved the same way as in flash-targets.cmake - see the shared
# ../../h7-common/cmake/probe-select.cmake (ninja flash-stlink/flash-dap/...).
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs    -DPYTHON=/path/to/python
#   -DQSPI_ALGO_PAGE_SIZE=0x4000    -DDEBUG_PROBE=VID:PID[:SERIAL]

include(${CMAKE_CURRENT_LIST_DIR}/probe-select.cmake)

set(QSPI_ALGO_PAGE_SIZE "0x4000" CACHE STRING
    "probe-rs page_size in the algorithm YAML (bytes per ProgramPage call)")

h7_tool_path("$ENV{LOCALAPPDATA}/Programs/Python" _PY_HINT1)
h7_tool_path("$ENV{USERPROFILE}/AppData/Local/Python" _PY_HINT2)

find_program(PYTHON NAMES python python3 py
    HINTS "${_PY_HINT1}" "${_PY_HINT2}"
    DOC "python interpreter (to build the flash algorithm YAML)")

set(QSPI_ALGO_DIR "${CMAKE_CURRENT_LIST_DIR}/../tool/qspi_map/algo")
set(QSPI_ALGO_SRC "${QSPI_ALGO_DIR}/flash_w25q64_qspi.c")
set(QSPI_ALGO_YAML "${QSPI_ALGO_DIR}/target_w25q64_qspi.yaml")
set(BIN_HEX "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex")

if(PROBE_RS AND PYTHON)
    # (Re)generate the algorithm YAML only when missing or older than the source.
    add_custom_command(
        OUTPUT "${QSPI_ALGO_YAML}"
        COMMAND "${PYTHON}" build_algo.py flash_w25q64_qspi.c ${QSPI_ALGO_PAGE_SIZE}
        WORKING_DIRECTORY "${QSPI_ALGO_DIR}"
        DEPENDS "${QSPI_ALGO_SRC}"
        COMMENT "Generating QUADSPI flash algorithm ${QSPI_ALGO_YAML} ...")

    foreach(_fam auto stlink dap jlink)
        if(_fam STREQUAL "auto")
            set(_tgt flash)
        else()
            set(_tgt flash-${_fam})
        endif()

        h7_probe_ready(${_fam} _ready)
        if(NOT _ready)
            h7_probe_hint(${_fam} _hint)
            add_custom_target(${_tgt}
                COMMAND ${CMAKE_COMMAND} -E echo "${_hint}"
                COMMAND ${CMAKE_COMMAND} -E false
                COMMENT "${_tgt}: no ${_fam} probe")
            continue()
        endif()

        h7_probe_args(${_fam} _pargs)
        if(_pargs)
            set(_how "${_fam} probe")
        else()
            set(_how "probe-rs auto-detect")
        endif()

        add_custom_target(${_tgt}
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "Make sure h750_boot is in internal flash first - one-time per board"
            COMMAND ${CMAKE_COMMAND} -E echo "Writing ${PROJECT_NAME}.hex to the W25Q64 via the QUADSPI algorithm ..."
            COMMAND "${PROBE_RS}" download ${_pargs}
                        --chip-description-path "${QSPI_ALGO_YAML}"
                        --chip "STM32H750VB-W25Q64-w25q64_qspi" --protocol swd
                        --binary-format hex --non-interactive --disable-progressbars
                        "${BIN_HEX}"
            COMMAND "${PROBE_RS}" reset ${_pargs}
                        --chip "STM32H750VB" --protocol swd --non-interactive
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "Flashing done - board reset, booting the new app."
            DEPENDS hex "${QSPI_ALGO_YAML}"
            COMMENT "Flashing ${PROJECT_NAME}.hex to the W25Q64 (probe-rs QUADSPI algorithm) [${_how}]"
            USES_TERMINAL)
    endforeach()
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo
                "probe-rs and/or python not found. Set -DPROBE_RS=/path/to/probe-rs and -DPYTHON=/path/to/python.")
endif()
