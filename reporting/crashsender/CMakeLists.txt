project(CrashSender)

# Create the list of source files
aux_source_directory( . source_files )
file( GLOB header_files *.h )

# Enable usage of precompiled header
set(srcs_using_precomp ${source_files})
list(REMOVE_ITEM srcs_using_precomp ./stdafx.cpp ./md5.cpp ./base64.cpp)
add_msvc_precompiled_header(stdafx.h ./stdafx.cpp srcs_using_precomp)

list(APPEND source_files
  ./CrashSender.rc
  ${CRASHRPT_SRC}/reporting/CrashRpt/Utility.cpp
  ${CRASHRPT_SRC}/reporting/CrashRpt/SharedMem.cpp
)

# Define _UNICODE and UNICODE (use wide-char encoding)
add_compile_definitions( _UNICODE UNICODE )

fix_default_compiler_settings_()

# Add include dir
include_directories(
  ${CRASHRPT_SRC}/include
  ${CRASHRPT_SRC}/reporting/CrashRpt
  ${CRASHRPT_SRC}/thirdparty/wtl
  ${CRASHRPT_SRC}/thirdparty/zlib
  ${CRASHRPT_SRC}/thirdparty/minizip
  ${CRASHRPT_SRC}/thirdparty/jpeg
  ${CRASHRPT_SRC}/thirdparty/libpng
  ${CRASHRPT_SRC}/thirdparty/tinyxml
  ${DBGHELP_INCLUDE_DIR}
  ${CRASHRPT_SRC}/thirdparty/libogg/include
  ${CRASHRPT_SRC}/thirdparty/libtheora/include
)

if(NOT CMAKE_CL_64)
  link_directories( ${CRASHRPT_SRC}/thirdparty/dbghelp/lib )
else(NOT CMAKE_CL_64)
  link_directories( ${CRASHRPT_SRC}/thirdparty/dbghelp/lib/amd64 )
endif(NOT CMAKE_CL_64)

# Add executable build target
add_executable(CrashSender WIN32 ${source_files} ${header_files})

# Add input link libraries
target_link_libraries(CrashSender
  zlib
  minizip
  libjpeg
  libpng
  tinyxml
  libogg
  libtheora
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

set_target_properties(CrashSender PROPERTIES
  DEBUG_POSTFIX ${CRASHRPT_VER}d
  RELEASE_POSTFIX ${CRASHRPT_VER}
  RELWITHDEBINFO_POSTFIX ${CRASHRPT_VER}
  MINSIZEREL_POSTFIX ${CRASHRPT_VER}
)

INSTALL(TARGETS CrashSender
  LIBRARY DESTINATION ${CRASHRPT_INSTALLDIR_BIN}
  ARCHIVE DESTINATION ${CRASHRPT_INSTALLDIR_LIB}
  RUNTIME DESTINATION ${CRASHRPT_INSTALLDIR_BIN}
)

if (CRASHRPT_INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:CrashSender> DESTINATION ${CRASHRPT_INSTALLDIR_BIN})
endif()
