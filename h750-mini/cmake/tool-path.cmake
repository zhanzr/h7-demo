# Shared helper for tool paths in custom-command lines.
#
# MSYS cmake is awkward with Windows-style paths: find_program() can hand back a
# re-rooted path ("C:/x/y.exe" becomes "<cwd>/C:/x/y.exe"), and its Ninja
# generator then writes /bin/sh scripts in which such a path is looked up
# relative to the build dir. h723_tool_path() repairs the re-rooted form and
# addresses the drive the way the active shell understands it: "/c/x/y.exe" for
# sh (MSYS cmake), "c:/x/y.exe" for cmd.exe (Windows/mingw cmake).
#
# Included by probe-select.cmake (flash targets) and stm32h723_board.cmake
# (starm-clang's multilib path).

if(NOT COMMAND h723_tool_path)
    function(h723_tool_path in out)
        set(_p "${in}")
        # Split at the first "X:/" marker: everything before it is a prefix MSYS
        # added by re-rooting the path against the current binary dir.
        string(FIND "${_p}" ":/" _c)
        if(_c GREATER -1)
            math(EXPR _dpos "${_c} - 1")
            math(EXPR _restpos "${_c} + 1")
            string(SUBSTRING "${_p}" ${_dpos} 1 _drive)
            string(SUBSTRING "${_p}" ${_restpos} -1 _rest)
            string(TOLOWER "${_drive}" _drive)
            if(CMAKE_COMMAND MATCHES "^/")
                set(_p "/${_drive}${_rest}")
            else()
                set(_p "${_drive}:${_rest}")
            endif()
        endif()
        set(${out} "${_p}" PARENT_SCOPE)
    endfunction()
endif()
