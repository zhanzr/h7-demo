# Shared flashing targets for the STM32H750VB (h750-mini board).
#
#   ninja flash          - probe-rs download over SWD; probe-rs auto-detects the
#                          connected probe (ST-Link / CMSIS-DAP / J-Link)
#   ninja flash-stlink   - force the attached ST-Link
#   ninja flash-dap      - force the attached CMSIS-DAP probe (DAPLink/mbed/...)
#   ninja flash-jlink    - force the attached SEGGER J-Link
#   ninja flash-ulink    - explains why ULINK cannot be automated here
#   ninja probes         - list the probes probe-rs can see right now
#
# The probe families are resolved from `probe-rs list` at configure time; see
# ../../h7-common/cmake/probe-select.cmake (shared with the other boards).
# Per-probe overrides:
#   -DDEBUG_PROBE=VID:PID[:SERIAL]    pin one probe for every target
#   PROBE_RS_PROBE=VID:PID[:SERIAL]   same, for a single run
#
# Note: the ULINK2 used with this board enumerates as a CMSIS-DAP probe, so it
# works through the `dap` family (or auto-detect); it cannot capture SWO, so the
# console is the USART1 UART (PA9/PA10, CH340).
#
# Internal flash: the part is binned to 128 KB but the die has 2 MB (shadow
# H743). probe-rs enforces the chip definition, so STM32H750VB is a safe
# guardrail; for images past 0x0801FFFF flash with -DPROBE_RS_CHIP=STM32H743VI
# (the 2 MB flash test does exactly that - see bare/h743_verify).
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs   -DPROBE_RS_CHIP=STM32H750VB (default)

include(${CMAKE_CURRENT_LIST_DIR}/probe-select.cmake)

set(PROBE_RS_CHIP "STM32H750VB" CACHE STRING "probe-rs target chip name")

# Custom chip definitions with page_size bumped from 1 KB to 16 KB: probe-rs's
# per-ProgramPage-call overhead dominated internal flashing (41 KB: ~9 s -> ~6.5 s;
# 1.9 MB on the shadow flash: 283 s -> 154 s). These YAMLs are chip-level, so
# they live in the shared h7-common/cmake tree.
set(H7_CHIP_YAML_DIR "${CMAKE_CURRENT_LIST_DIR}/../../h7-common/cmake")
set(FLASH_CHIP_ARGS)
if(PROBE_RS_CHIP STREQUAL "STM32H750VB" AND EXISTS "${H7_CHIP_YAML_DIR}/stm32h750_custom.yaml")
    set(FLASH_CHIP_ARGS --chip-description-path "${H7_CHIP_YAML_DIR}/stm32h750_custom.yaml")
elseif(PROBE_RS_CHIP STREQUAL "STM32H743VI" AND EXISTS "${H7_CHIP_YAML_DIR}/stm32h743_custom.yaml")
    set(FLASH_CHIP_ARGS --chip-description-path "${H7_CHIP_YAML_DIR}/stm32h743_custom.yaml")
endif()

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
            COMMAND "${PROBE_RS}" download ${_pargs}
                        ${FLASH_CHIP_ARGS}
                        --chip "${PROBE_RS_CHIP}" --protocol swd
                        --binary-format hex --verify --reset --non-interactive
                        --disable-progressbars "${BIN_HEX}"
            DEPENDS hex
            COMMENT "Flashing ${PROJECT_NAME}.hex to ${PROBE_RS_CHIP} via probe-rs [${_how}]"
            USES_TERMINAL)
    endforeach()

    h7_add_ulink_stub(flash)
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo "probe-rs not found - install it with cargo install probe-rs-tools, or pass -DPROBE_RS=/path/to/probe-rs")
endif()
