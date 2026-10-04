# ===========================================================================
# Shared debug-probe discovery for every `ninja flash*` target.
#
# Included by cmake/flash-targets.cmake, cmake/qspi-flash-targets.cmake and
# tool/qspi_map/CMakeLists.txt. Each of those creates its own flash targets
# once per probe family, using the helpers below.
#
# Target created here (once per project):
#   ninja probes         list the connected debug probes (`probe-rs list`)
#
# Target naming used by the flash targets:
#   ninja flash          / ninja flash_boot        - probe-rs auto-detects the
#                                                    connected probe
#   ninja flash-stlink   / ninja flash_boot-stlink - the attached ST-Link
#   ninja flash-dap      / ninja flash_boot-dap    - the attached CMSIS-DAP
#   ninja flash-jlink    / ninja flash_boot-jlink  - the attached SEGGER J-Link
#   ninja flash-ulink    / ninja flash_boot-ulink  - guidance only: probe-rs has
#                                                    no ULINK driver
#
# Automatic probe selection:
#   * the plain `flash` target omits --probe, so probe-rs picks the probe itself
#     (it errors if several are attached - use a family target then).
#   * <family> targets use the first probe of that family seen by `probe-rs list`
#     at configure time, addressed as VID:PID:SERIAL when the probe reports a
#     serial number, else VID:PID.
#   * -DDEBUG_PROBE=VID:PID[:SERIAL] overrides every target (any probe, any
#     family).
#   * PROBE_RS_PROBE=VID:PID[:SERIAL] in the environment overrides one run.
#   Family selectors are resolved at configure time, so after swapping probes
#   either re-run cmake (e.g. `cmake .` inside the build dir) or pass
#   -DDEBUG_PROBE=... / PROBE_RS_PROBE=... .
# ===========================================================================

if(H723_PROBE_SELECT_INCLUDED)
    return()
endif()
set(H723_PROBE_SELECT_INCLUDED TRUE)

# h723_tool_path() lives in tool-path.cmake (shared with stm32h723_board.cmake).
include(${CMAKE_CURRENT_LIST_DIR}/tool-path.cmake)

find_program(PROBE_RS NAMES probe-rs probe-rs.exe
    HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{CARGO_HOME}/bin"
    DOC "probe-rs binary (SWD flasher for ST-Link / CMSIS-DAP / J-Link)")
if(PROBE_RS)
    h723_tool_path("${PROBE_RS}" PROBE_RS)
endif()

set(DEBUG_PROBE "" CACHE STRING
    "probe-rs --probe selector (VID:PID[:Serial]); empty = auto-detect / per-family default")

# --- resolve the attached probes once, at configure time -------------------
set(H723_PROBE_STLINK "")
set(H723_PROBE_DAP "")
set(H723_PROBE_JLINK "")
set(H723_PROBE_FOUND "")
set(H723_ALL_SELECTORS "")
set(H723_SELS_STLINK "")
set(H723_SELS_DAP "")
set(H723_SELS_JLINK "")

if(PROBE_RS)
    # `probe-rs list`, tolerant of the cmake flavour: the MSYS build of cmake
    # mangles Windows-style paths inside execute_process (it re-roots them), so
    # fall back to the POSIX form of the path and finally to a PATH lookup.
    set(_h723_list "")
    execute_process(COMMAND "${PROBE_RS}" list
        OUTPUT_VARIABLE _h723_list ERROR_QUIET RESULT_VARIABLE _h723_rc)
    if(NOT _h723_list AND PROBE_RS MATCHES "^[A-Za-z]:/")
        string(SUBSTRING "${PROBE_RS}" 0 1 _drive)
        string(TOLOWER "${_drive}" _drive)
        string(SUBSTRING "${PROBE_RS}" 2 -1 _rest)
        execute_process(COMMAND "/${_drive}${_rest}" list
            OUTPUT_VARIABLE _h723_list ERROR_QUIET RESULT_VARIABLE _h723_rc)
    endif()
    if(NOT _h723_list)
        execute_process(COMMAND probe-rs list
            OUTPUT_VARIABLE _h723_list ERROR_QUIET RESULT_VARIABLE _h723_rc)
    endif()
    if(NOT _h723_list)
        message(WARNING "could not run 'probe-rs list' from CMake here - configure-time probe detection is unavailable, so every flash-<family> target will report its probe as missing. Pass -DDEBUG_PROBE=VID:PID or VID:PID:SERIAL to select one explicitly.")
    endif()
    if(_h723_rc EQUAL 0)
        string(REPLACE "\r" "" _h723_list "${_h723_list}")
        string(REPLACE "\n" ";" _h723_lines "${_h723_list}")
        foreach(_line IN LISTS _h723_lines)
            # Typical line: "[0]: STLink V2 -- 0483:3748: (ST-LINK)"
            # Parsed with string ops, not regex: CMake's regex has no {n} support.
            string(FIND "${_line}" " -- " _dash)
            if(_dash EQUAL -1)
                continue()
            endif()
            math(EXPR _after "${_dash} + 4")
            string(SUBSTRING "${_line}" ${_after} -1 _rest)
            # _rest is "0483:3748: (ST-LINK)" or "1366:0101:000012345678 (J-Link)"
            set(_cut -1)
            foreach(_sep IN ITEMS " " "(" ")")
                string(FIND "${_rest}" "${_sep}" _i)
                if(_i GREATER -1 AND (_cut EQUAL -1 OR _i LESS _cut))
                    set(_cut ${_i})
                endif()
            endforeach()
            if(_cut GREATER -1)
                string(SUBSTRING "${_rest}" 0 ${_cut} _id)
            else()
                set(_id "${_rest}")
            endif()
            string(REPLACE ":" ";" _parts "${_id}")
            list(LENGTH _parts _n)
            if(_n LESS 2)
                continue()
            endif()
            list(GET _parts 0 _vid)
            list(GET _parts 1 _pid)
            set(_ser "")
            if(_n GREATER 2)
                list(GET _parts 2 _ser)
            endif()

            string(TOLOWER "${_line}" _ll)
            string(TOLOWER "${_vid}" _vidl)
            set(_fam "")
            if(_ll MATCHES "st-?link" OR _vidl STREQUAL "0483")
                set(_fam stlink)
            elseif(_ll MATCHES "cmsis|dap")
                set(_fam dap)
            elseif(_ll MATCHES "j-?link|segger" OR _vidl STREQUAL "1366")
                set(_fam jlink)
            endif()
            if(_fam)
                if(_ser)
                    set(_sel "${_vid}:${_pid}:${_ser}")
                else()
                    set(_sel "${_vid}:${_pid}")
                endif()
                string(TOUPPER "${_fam}" _FAM)
                list(APPEND H723_ALL_SELECTORS "${_sel}")
                list(APPEND H723_SELS_${_FAM} "${_sel}")
                if(NOT H723_PROBE_${_FAM})
                    set(H723_PROBE_${_FAM} "${_sel}")
                    string(APPEND H723_PROBE_FOUND "\n    ${_fam}: ${_sel}    <- ${_line}")
                endif()
            endif()
        endforeach()
    endif()
endif()

if(H723_PROBE_FOUND)
    message(STATUS "Debug probes seen by probe-rs:${H723_PROBE_FOUND}")
else()
    message(STATUS "probe-rs sees no debug probe right now - `ninja flash` still auto-detects at run time")
endif()

# Several probes of one family: only the first is used, so say so.
foreach(_fam stlink dap jlink)
    string(TOUPPER "${_fam}" _FAM)
    list(LENGTH H723_SELS_${_FAM} _n)
    if(_n GREATER 1)
        message(WARNING "${_n} ${_fam} probes are attached (${H723_SELS_${_FAM}}) - the ${_fam} targets use the first one. Pass -DDEBUG_PROBE=VID:PID[:SERIAL] to choose another.")
    endif()
endforeach()

# A pinned probe that is not attached would make every flash target fail with
# "No connected probes were found", so report it and fall back to per-target
# selection. Clear the stale pin for good with -DDEBUG_PROBE= .
if(DEBUG_PROBE)
    string(TOLOWER "${DEBUG_PROBE}" _h723_pin)
    set(_h723_pin_ok FALSE)
    foreach(_sel IN LISTS H723_ALL_SELECTORS)
        string(TOLOWER "${_sel}" _s)
        if(_s STREQUAL _h723_pin OR _s MATCHES "^${_h723_pin}:")
            set(_h723_pin_ok TRUE)
        endif()
    endforeach()
    if(_h723_pin_ok)
        message(STATUS "DEBUG_PROBE=${DEBUG_PROBE} is attached - every flash target uses it")
    else()
        message(WARNING "DEBUG_PROBE=${DEBUG_PROBE} is not attached right now - ignoring it; each flash target will use the probe it is named for. Clear the stale pin with -DDEBUG_PROBE= (or delete CMakeCache.txt).")
        set(DEBUG_PROBE "")
    endif()
endif()

# --- `ninja probes` --------------------------------------------------------
if(PROBE_RS)
    if(NOT TARGET probes)
        add_custom_target(probes
            COMMAND "${PROBE_RS}" list
            COMMENT "Listing connected debug probes"
            USES_TERMINAL)
    endif()
else()
    message(WARNING "probe-rs not found - the flash targets will not work (install with `cargo install probe-rs-tools` or pass -DPROBE_RS=/path/to/probe-rs)")
endif()

# --- helpers ---------------------------------------------------------------

# h723_probe_args(<family> <out_var>): the `--probe <selector>` args for a
# family; empty for "auto" (let probe-rs choose).
function(h723_probe_args family out)
    if(DEBUG_PROBE)
        set(${out} --probe "${DEBUG_PROBE}" PARENT_SCOPE)
    elseif(family STREQUAL "auto")
        set(${out} "" PARENT_SCOPE)
    else()
        string(TOUPPER "${family}" _F)
        set(_sel "${H723_PROBE_${_F}}")
        if(_sel)
            set(${out} --probe "${_sel}" PARENT_SCOPE)
        else()
            set(${out} "" PARENT_SCOPE)
        endif()
    endif()
endfunction()

# h723_probe_ready(<family> <out_var>): TRUE when that family can be used.
function(h723_probe_ready family out)
    if(DEBUG_PROBE OR family STREQUAL "auto")
        set(${out} TRUE PARENT_SCOPE)
        return()
    endif()
    string(TOUPPER "${family}" _F)
    if(H723_PROBE_${_F})
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

# h723_probe_hint(<family> <out_var>): one line telling the user what to attach.
function(h723_probe_hint family out)
    if(family STREQUAL "stlink")
        set(_what "an ST-Link - V2 0483:3748, V2-1 0483:374b, V3 0483:374e/374f")
    elseif(family STREQUAL "dap")
        set(_what "a CMSIS-DAP probe - DAPLink, mbed, or a debugger in CMSIS-DAP mode")
    elseif(family STREQUAL "jlink")
        set(_what "a SEGGER J-Link - 1366:0101/0105/1015/...")
    else()
        set(_what "a ${family} probe")
    endif()
    set(${out} "no ${family} probe was detected at configure time - connect ${_what}, then re-run cmake or pass -DDEBUG_PROBE=VID:PID or VID:PID:SERIAL" PARENT_SCOPE)
endfunction()

# h723_add_ulink_stub(<base_target>): <base>-ulink explains why ULINK cannot be
# automated from this repo, then fails.
#
# ULINK is Keil-proprietary: probe-rs has no driver for it, and neither has
# openocd nor pyOCD. Driving it needs Keil uVision itself (installed here as
# D:/Keil_v5/UV4/UV4.exe) plus a .uvprojx whose Debug/Flash Download settings use
# the ULINK2 - i.e. a Keil project with a flash algorithm for the target. For the
# external W25Q64 that would also need a Keil .FLM, which this repo does not have
# (it ships a probe-rs algorithm instead).
function(h723_add_ulink_stub base)
    add_custom_target(${base}-ulink
        COMMAND ${CMAKE_COMMAND} -E echo "ULINK is not supported by probe-rs - there is no ULINK driver - so CMake cannot flash it."
        COMMAND ${CMAKE_COMMAND} -E echo "Use one of: ninja ${base} , ninja ${base}-stlink , ninja ${base}-dap , ninja ${base}-jlink"
        COMMAND ${CMAKE_COMMAND} -E echo "For the ULINK2 itself use Keil uVision - open a .uvprojx whose Flash Download settings use the ULINK2, then Flash - Download"
        COMMAND ${CMAKE_COMMAND} -E echo "uVision is installed at D:/Keil_v5/UV4/UV4.exe - a ULINK2 did not reach this board before either, see cmake/flash-targets.cmake"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "${base}-ulink: unsupported"
        USES_TERMINAL)
endfunction()
