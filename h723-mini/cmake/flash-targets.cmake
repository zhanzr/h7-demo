# Shared flashing targets for the STM32H723ZG (h723-mini board).
#
#   ninja flash      - probe-rs download over SWD. probe-rs auto-detects the
#                      connected probe (a single ST-Link V2 works); pass
#                      -DDEBUG_PROBE=<selector> to pin a specific probe.
#                      A ULINK2 unit on hand did not access this board (a local
#                      observation, not a rigid test), so the ST-Link V2 is used.
#   ninja dfu-flash  - STM32CubeProgrammer USB DFU download (fallback; needs
#                      the board in DFU mode: BOOT0=1 + reset + USB).
#
# Note: the probe cannot capture SWO (the ST-Link VCP UART is the console).
#
# How to view a probe's selector (VID:PID:Serial): run `probe-rs list` with
# the probe plugged in (e.g. `0483:374b:xxxx...` for an ST-Link V2).
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs   -DDEBUG_PROBE=<probe selector>
#   -DPROBE_RS_CHIP=STM32H723ZG    (default)
#   -DSTM32_PROG=/path/to/STM32_Programmer_CLI.exe

set(DEBUG_PROBE "" CACHE STRING
    "probe-rs --probe selector (VID:PID[:Serial], e.g. 0483:374b:xxxx...); empty = auto-detect")

# When DEBUG_PROBE is empty, omit --probe so probe-rs auto-detects the probe.
if(DEBUG_PROBE)
    set(PROBE_ARGS --probe "${DEBUG_PROBE}")
else()
    set(PROBE_ARGS)
endif()

set(PROBE_RS_CHIP "STM32H723ZG" CACHE STRING "probe-rs target chip name")

find_program(PROBE_RS NAMES probe-rs probe-rs.exe
    HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{CARGO_HOME}/bin"
    DOC "probe-rs binary (preferred SWD flasher)")

set(BIN_HEX "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex")

if(PROBE_RS)
    add_custom_target(flash
        COMMAND "${PROBE_RS}" download ${PROBE_ARGS}
                    --chip "${PROBE_RS_CHIP}" --protocol swd
                    --binary-format hex --verify --reset --non-interactive
                    --disable-progressbars "${BIN_HEX}"
        DEPENDS hex
        COMMENT "Flashing ${PROJECT_NAME}.hex to ${PROBE_RS_CHIP} via probe-rs (${DEBUG_PROBE}) ..."
        USES_TERMINAL)
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo "probe-rs not found. Install it (cargo install probe-rs-tools) or pass -DPROBE_RS=/path/to/probe-rs.")
endif()

# ---------------------------------------------------------------------------
# USB DFU flashing via STM32CubeProgrammer (works even when SWD is blocked).
find_program(STM32_PROG NAMES STM32_Programmer_CLI STM32_Programmer_CLI.exe
    HINTS "D:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin"
    DOC "STM32CubeProgrammer CLI (USB DFU flasher)")

if(STM32_PROG)
    add_custom_target(dfu-flash
        COMMAND "${STM32_PROG}" -c port=USB1 mode=DFU -d "${BIN_HEX}" -v
        DEPENDS hex
        COMMENT "Flashing ${PROJECT_NAME}.hex via USB DFU (put board in DFU mode first: BOOT0=1 + reset, USB connected) ..."
        USES_TERMINAL)
else()
    add_custom_target(dfu-flash
        COMMAND ${CMAKE_COMMAND} -E echo "STM32_Programmer_CLI not found. Install STM32CubeProgrammer or pass -DSTM32_PROG=/path/to/STM32_Programmer_CLI.exe.")
endif()
