project(crashsender)

# Create the list of source files
aux_source_directory( . SOURCE_FILE_LIST )
file( GLOB HEADER_FILE_LIST *.h )

# repository directory
cmake_path(SET REPOSITORY_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Enable usage of precompiled header
set(srcs_using_precomp ${SOURCE_FILE_LIST})
list(REMOVE_ITEM srcs_using_precomp ./stdafx.cpp ./md5.cpp ./base64.cpp)
add_msvc_precompiled_header(stdafx.h ./stdafx.cpp srcs_using_precomp)

list(APPEND SOURCE_FILE_LIST ./CrashSender.rc)

# Define _UNICODE and UNICODE (use wide-char encoding)
add_compile_definitions( _UNICODE UNICODE )

# Add include dir
include_directories(
  ${REPOSITORY_DIR}/include
  ${REPOSITORY_DIR}/thirdparty/wtl
  ${REPOSITORY_DIR}/thirdparty/zlib
  ${REPOSITORY_DIR}/thirdparty/minizip
  ${REPOSITORY_DIR}/thirdparty/tinyxml
  ${DBGHELP_INCLUDE_DIR}
)

if(NOT CMAKE_CL_64)
  link_directories( ${REPOSITORY_DIR}/thirdparty/dbghelp/lib )
else(NOT CMAKE_CL_64)
  link_directories( ${REPOSITORY_DIR}/thirdparty/dbghelp/lib/amd64 )
endif(NOT CMAKE_CL_64)

# Add executable build target
add_executable(crashsender WIN32 ${SOURCE_FILE_LIST} ${HEADER_FILE_LIST})

# Add input link libraries
target_link_libraries(crashsender
  zlib
  minizip
  tinyxml
  WS2_32.lib
  Dnsapi.lib
  wininet.lib
  Rpcrt4.lib
  Gdi32.lib
  shell32.lib
  Comdlg32.lib
  version.lib
  psapi.lib
)

set_target_properties(crashsender PROPERTIES
  DEBUG_POSTFIX ${CRASHRPT_VER}d
  RELEASE_POSTFIX ${CRASHRPT_VER}
  RELWITHDEBINFO_POSTFIX ${CRASHRPT_VER}
  MINSIZEREL_POSTFIX ${CRASHRPT_VER}
)
