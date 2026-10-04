# objcopy/hex/bin targets
#
# The real file lives in the repo-level h7-common/cmake tree, shared by every
# board; only board-specific modules stay in this folder.
include("${CMAKE_CURRENT_LIST_DIR}/../../h7-common/cmake/objcopy-targets.cmake")
