project(crashrptprobe)

# source files
aux_source_directory( . SOURCE_FILE_LIST )
file( GLOB HEADER_FILE_LIST *.h )

# repository directory
cmake_path(SET REPOSITORY_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../..")

list(APPEND SOURCE_FILE_LIST
  ./CrashRptProbe.rc
  ./CrashRptProbe.def
)

# Enable usage of precompiled header
set(srcs_using_precomp ${SOURCE_FILE_LIST})
list(REMOVE_ITEM srcs_using_precomp  ./CrashRptProbe.rc ./CrashRptProbe.def ./stdafx.cpp ${REPOSITORY_DIR}/reporting/crashsender/md5.cpp)
add_msvc_precompiled_header(stdafx.h ./stdafx.cpp srcs_using_precomp)

# Define _UNICODE and UNICODE (use wide-char encoding)
add_compile_definitions( _UNICODE UNICODE )

# Add include dir
include_directories(
  ${REPOSITORY_DIR}/include
  ${REPOSITORY_DIR}/thirdparty/wtl
  ${REPOSITORY_DIR}/thirdparty/zlib
  ${REPOSITORY_DIR}/thirdparty/minizip
  ${REPOSITORY_DIR}/thirdparty/tinyxml
  ${DBGHELP_INCLUDE_DIR})

# Add library build target
if(CRASHRPT_BUILD_SHARED_LIBS)
  add_library(crashrptprobe SHARED ${SOURCE_FILE_LIST} ${HEADER_FILE_LIST})
endif()

if(CMAKE_CL_64)
  link_directories( ${REPOSITORY_DIR}/thirdparty/dbghelp/lib/amd64 )
  target_link_libraries(crashrptprobe ${REPOSITORY_DIR}/thirdparty/dbghelp/lib/amd64/dbghelp.lib)
else(CMAKE_CL_64)
  link_directories( ${REPOSITORY_DIR}/thirdparty/dbghelp/lib )
  target_link_libraries(crashrptprobe ${REPOSITORY_DIR}/thirdparty/dbghelp/lib/dbghelp.lib)
endif(CMAKE_CL_64)

target_link_libraries(crashrptprobe zlib minizip tinyxml Rpcrt4.lib shell32.lib gdi32.lib version.lib psapi.lib)

if(CRASHRPT_BUILD_SHARED_LIBS)
  set_target_properties(crashrptprobe PROPERTIES
                  DEBUG_POSTFIX ${CRASHRPT_VER}d
                  RELEASE_POSTFIX ${CRASHRPT_VER}
                  RELWITHDEBINFO_POSTFIX ${CRASHRPT_VER}
                  MINSIZEREL_POSTFIX ${CRASHRPT_VER})
endif()
