# Internal-flash flashing target for this project, using the shared probe
# discovery (see h7-common/cmake/probe-select.cmake): probe-rs auto-detects
# the attached probe (ST-Link / CMSIS-DAP / J-Link).
#
# Target:
#   ninja flash   - probe-rs download over SWD (probe auto-detected)
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs   -DDEBUG_PROBE=VID:PID[:SERIAL]
set(PROBE_RS_CHIP "STM32H750VB" CACHE STRING "probe-rs target chip name")

find_program(PROBE_RS NAMES probe-rs probe-rs.exe
    HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{CARGO_HOME}/bin"
    DOC "probe-rs binary (preferred flasher)")

include("${CMAKE_CURRENT_LIST_DIR}/../../../../h7-common/cmake/probe-select.cmake")
h7_probe_args(auto _pargs)

set(BIN_HEX "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.hex")

if(PROBE_RS)
    add_custom_target(flash
        COMMAND "${PROBE_RS}" download ${_pargs}
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
