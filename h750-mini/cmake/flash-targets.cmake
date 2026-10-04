# Shared flashing target for the STM32H750 (h750-mini board), programmed
# through the Keil ULINK2, which enumerates as a CMSIS-DAP probe.
#
# Target:
#   ninja flash   - probe-rs download (ULINK2 seen as CMSIS-DAP, SWD)
#
# Note: the ULINK2's CMSIS-DAP firmware cannot capture SWO, so there is no
# `swv` target here — the console is the USART1 UART (PA9/PA10, CH340).
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs   -DULINK2_PROBE=c251:2722:V0010M9E
#   -DPROBE_RS_CHIP=STM32H743VI    (e.g. for the h743_verify 2 MB-flash test)

set(ULINK2_PROBE "c251:2722:V0010M9E" CACHE STRING
    "probe-rs --probe selector (VID:PID[:Serial]) of the Keil ULINK2")
set(PROBE_RS_CHIP "STM32H750VB" CACHE STRING "probe-rs target chip name")

find_program(PROBE_RS NAMES probe-rs probe-rs.exe
    HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{CARGO_HOME}/bin"
    DOC "probe-rs binary (preferred flasher)")

set(BIN_HEX "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex")

# Custom chip definitions that bump the internal-flash algorithm's page_size
# from 1 KB (probe-rs default) to 16 KB. probe-rs's per-ProgramPage-call
# overhead dominated the flash time (same trick as the W25Q64 algorithm);
# measured: 41 KB ~9 s -> ~6.5 s; 1.9 MB on the 2 MiB shadow flash 283 s ->
# 154 s. Each custom YAML contains exactly the one chip variant it names.
set(STM32H750_CUSTOM_YAML "${CMAKE_CURRENT_LIST_DIR}/stm32h750_custom.yaml")
set(STM32H743_CUSTOM_YAML "${CMAKE_CURRENT_LIST_DIR}/stm32h743_custom.yaml")

if(PROBE_RS)
    set(FLASH_CHIP_ARGS)
    if(PROBE_RS_CHIP STREQUAL "STM32H750VB" AND EXISTS "${STM32H750_CUSTOM_YAML}")
        set(FLASH_CHIP_ARGS --chip-description-path "${STM32H750_CUSTOM_YAML}")
    elseif(PROBE_RS_CHIP STREQUAL "STM32H743VI" AND EXISTS "${STM32H743_CUSTOM_YAML}")
        set(FLASH_CHIP_ARGS --chip-description-path "${STM32H743_CUSTOM_YAML}")
    endif()
    add_custom_target(flash
        COMMAND "${PROBE_RS}" download --probe "${ULINK2_PROBE}"
                    ${FLASH_CHIP_ARGS}
                    --chip "${PROBE_RS_CHIP}" --protocol swd
                    --connect-under-reset
                    --binary-format hex --verify --reset --non-interactive
                    --disable-progressbars "${BIN_HEX}"
        DEPENDS hex
        COMMENT "Flashing ${PROJECT_NAME}.hex to ${PROBE_RS_CHIP} via probe-rs (ULINK2 CMSIS-DAP, SWD) ..."
        USES_TERMINAL)
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo "probe-rs not found. Install it (cargo install probe-rs-tools) or pass -DPROBE_RS=/path/to/probe-rs.")
endif()
