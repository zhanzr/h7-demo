# Shared flashing target for the STM32H750 (h750-mini board), programmed
# through the Keil ULINK2, which enumerates as a CMSIS-DAP probe.
#
# Target:
#   ninja flash   - probe-rs download (ULINK2 seen as CMSIS-DAP, SWD)
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs   -DULINK2_PROBE=c251:2722:V0010M9E

set(ULINK2_PROBE "c251:2722:V0010M9E" CACHE STRING
    "probe-rs --probe selector (VID:PID[:Serial]) of the Keil ULINK2")
set(PROBE_RS_CHIP "STM32H750VB" CACHE STRING "probe-rs target chip name")

find_program(PROBE_RS NAMES probe-rs probe-rs.exe
    HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{CARGO_HOME}/bin"
    DOC "probe-rs binary (preferred flasher)")

set(BIN_HEX "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.hex")

if(PROBE_RS)
    add_custom_target(flash
        COMMAND "${PROBE_RS}" download --probe "${ULINK2_PROBE}"
                    --chip "${PROBE_RS_CHIP}" --protocol swd
                    --binary-format hex --verify --reset --non-interactive
                    --disable-progressbars "${BIN_HEX}"
        DEPENDS ${PROJECT_NAME}.elf
        COMMENT "Flashing ${PROJECT_NAME}.hex to STM32H750VB via probe-rs (ULINK2 CMSIS-DAP, SWD) ..."
        USES_TERMINAL)
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo "probe-rs not found. Install it (cargo install probe-rs-tools) or pass -DPROBE_RS=/path/to/probe-rs.")
endif()
