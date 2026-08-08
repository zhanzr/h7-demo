# Shared `ninja flash` target for pure-QSPI (_qspi) apps on the h723-mini.
#
# Unlike flash-targets.cmake (which programs INTERNAL flash), this flashes the
# app .hex into the on-board W25Q64 at 0x90000000 using the probe-rs OCTOSPI
# flash algorithm in ../qspi_map/algo/ (target_w25q64_ospi.yaml).
#
# The algorithm driver is auto-detected: if target_w25q64_ospi.yaml is missing
# or older than flash_w25q64_ospi.c, `build_algo.py` is run first to regenerate
# it, so no manual setup step is needed.
#
# Target:
#   ninja flash - build the .hex, (re)build the algorithm if needed, then write
#                 it to the W25Q64 via probe-rs (ST-Link V2, SWD)
#
# Prerequisite (one-time per board): h723_boot must be in internal flash.
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs    -DDEBUG_PROBE=<probe selector>
#   -DPYTHON=/path/to/python        -DQSPI_ALGO_PAGE_SIZE=0x4000
#
# DEBUG_PROBE is optional: empty (default) makes probe-rs auto-detect the
# connected probe. To pin a specific probe, view its selector with `probe-rs
# list` (e.g. 0483:374b:xxxx... for an ST-Link V2) and pass -DDEBUG_PROBE=...
set(DEBUG_PROBE "" CACHE STRING
    "probe-rs --probe selector (VID:PID[:Serial], e.g. 0483:374b:xxxx...); empty = auto-detect")

# When DEBUG_PROBE is empty, omit --probe so probe-rs auto-detects the probe.
if(DEBUG_PROBE)
    set(PROBE_ARGS --probe "${DEBUG_PROBE}")
else()
    set(PROBE_ARGS)
endif()
set(QSPI_ALGO_PAGE_SIZE "0x4000" CACHE STRING
    "probe-rs page_size in the algorithm YAML (bytes per ProgramPage call)")

find_program(PROBE_RS NAMES probe-rs probe-rs.exe
    HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{CARGO_HOME}/bin"
    DOC "probe-rs binary (preferred flasher)")

find_program(PYTHON NAMES python python3 py
    HINTS "$ENV{LOCALAPPDATA}/Programs/Python"
          "$ENV{USERPROFILE}/AppData/Local/Python"
    DOC "python interpreter (to build the flash algorithm YAML)")

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

    # ANSI color for the bootloader warning (harmless if the terminal is dumb).
    string(ASCII 27 ESC)
    set(WARN "${ESC}[1;33m")     # bold yellow
    set(NORM "${ESC}[0m")

    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo ""      # blank line (separate from ninja's status line)
        COMMAND ${CMAKE_COMMAND} -E echo "${WARN}Make sure h723_boot is in internal flash first (one-time per board)!${NORM}"
        COMMAND ${CMAKE_COMMAND} -E echo
                "Writing ${PROJECT_NAME}.hex to the W25Q64 via the OCTOSPI algorithm ..."
        COMMAND "${PROBE_RS}" download ${PROBE_ARGS}
                    --chip-description-path "${QSPI_ALGO_YAML}"
                    --chip "STM32H723ZG-W25Q64-w25q64_ospi" --protocol swd
                    --connect-under-reset
                    --binary-format hex --non-interactive --disable-progressbars
                    "${BIN_HEX}"
        COMMAND "${PROBE_RS}" reset ${PROBE_ARGS}
                    --chip "STM32H723ZG" --protocol swd
                    --connect-under-reset --non-interactive
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "Flashing done; board reset - booting the new app."
        DEPENDS hex "${QSPI_ALGO_YAML}"
        COMMENT "Flashing ${PROJECT_NAME}.hex to the W25Q64 (probe-rs OCTOSPI algorithm)"
        USES_TERMINAL)
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo
                "probe-rs and/or python not found. Set -DPROBE_RS=/path/to/probe-rs and -DPYTHON=/path/to/python.")
endif()
