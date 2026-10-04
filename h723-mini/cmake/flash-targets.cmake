# Shared flashing targets for the STM32H723ZG (h723-mini board).
#
#   ninja flash          - probe-rs download over SWD; probe-rs auto-detects the
#                          connected probe (a single ST-Link V2 works).
#   ninja flash-stlink   - force the attached ST-Link
#   ninja flash-dap      - force the attached CMSIS-DAP probe (DAPLink/mbed/...)
#   ninja flash-jlink    - force the attached SEGGER J-Link
#   ninja flash-ulink    - explains why ULINK cannot be automated here
#   ninja probes         - list the probes probe-rs can see right now
#   ninja dfu-flash      - STM32CubeProgrammer USB DFU download (fallback; needs
#                          the board in DFU mode: BOOT0=1 + reset + USB).
#
# The probe families are resolved from `probe-rs list` at configure time; see
# cmake/probe-select.cmake. Per-probe overrides:
#   -DDEBUG_PROBE=VID:PID[:SERIAL]    pin one probe for every target
#   PROBE_RS_PROBE=VID:PID[:SERIAL]   same, for a single run
#
# Note: the probe cannot capture SWO (the ST-Link VCP UART is the console).
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs   -DPROBE_RS_CHIP=STM32H723ZG (default)
#   -DSTM32_PROG=/path/to/STM32_Programmer_CLI.exe

include(${CMAKE_CURRENT_LIST_DIR}/probe-select.cmake)

set(PROBE_RS_CHIP "STM32H723ZG" CACHE STRING "probe-rs target chip name")

set(BIN_HEX "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex")

if(PROBE_RS)
    # One target per probe family. The command line is spelled out in full at
    # each add_custom_target() call (rather than shared through a variable) so
    # CMake keeps every argument intact, including quoted ones containing ';'.
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
            COMMAND "${PROBE_RS}" download ${_pargs}
                        --chip "${PROBE_RS_CHIP}" --protocol swd
                        --binary-format hex --verify --reset --non-interactive
                        --disable-progressbars "${BIN_HEX}"
            DEPENDS hex
            COMMENT "Flashing ${PROJECT_NAME}.hex to ${PROBE_RS_CHIP} via probe-rs [${_how}]"
            USES_TERMINAL)
    endforeach()

    h723_add_ulink_stub(flash)
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo "probe-rs not found - install it with cargo install probe-rs-tools, or pass -DPROBE_RS=/path/to/probe-rs")
endif()

# ---------------------------------------------------------------------------
# USB DFU flashing via STM32CubeProgrammer (works even when SWD is blocked).
find_program(STM32_PROG NAMES STM32_Programmer_CLI STM32_Programmer_CLI.exe
    HINTS "D:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin"
    DOC "STM32CubeProgrammer CLI (USB DFU flasher)")
if(STM32_PROG)
    h723_tool_path("${STM32_PROG}" STM32_PROG)
endif()

if(STM32_PROG)
    add_custom_target(dfu-flash
        COMMAND "${STM32_PROG}" -c port=USB1 mode=DFU -d "${BIN_HEX}" -v
        DEPENDS hex
        COMMENT "Flashing ${PROJECT_NAME}.hex via USB DFU (put board in DFU mode first: BOOT0=1 + reset, USB connected) ..."
        USES_TERMINAL)
else()
    add_custom_target(dfu-flash
        COMMAND ${CMAKE_COMMAND} -E echo "STM32_Programmer_CLI not found - install STM32CubeProgrammer or pass -DSTM32_PROG=/path/to/STM32_Programmer_CLI.exe")
endif()
