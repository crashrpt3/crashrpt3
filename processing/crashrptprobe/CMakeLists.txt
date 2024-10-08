project(CrashRptProbe)

# Create the list of source files
aux_source_directory( . source_files )
file( GLOB header_files *.h )

list(APPEND source_files
  ./CrashRptProbe.rc
  ./CrashRptProbe.def
  ${CRASHRPT_SRC}/reporting/crashrpt/Utility.cpp
  ${CRASHRPT_SRC}/reporting/crashsender/md5.cpp
)

# Enable usage of precompiled header
set(srcs_using_precomp ${source_files})
list(REMOVE_ITEM srcs_using_precomp  ./CrashRptProbe.rc ./CrashRptProbe.def ./stdafx.cpp ${CRASHRPT_SRC}/reporting/crashsender/md5.cpp)
add_msvc_precompiled_header(stdafx.h ./stdafx.cpp srcs_using_precomp)

# Define _UNICODE and UNICODE (use wide-char encoding)
add_compile_definitions( _UNICODE UNICODE )

fix_default_compiler_settings_()

# Add include dir
include_directories( ${CRASHRPT_SRC}/include
      ${CRASHRPT_SRC}/reporting/crashrpt
      ${CRASHRPT_SRC}/reporting/crashsender
      ${CRASHRPT_SRC}/thirdparty/wtl
      ${CRASHRPT_SRC}/thirdparty/zlib
      ${CRASHRPT_SRC}/thirdparty/minizip
      ${CRASHRPT_SRC}/thirdparty/tinyxml
      ${DBGHELP_INCLUDE_DIR})

# Add library build target
if(CRASHRPT_BUILD_SHARED_LIBS)
  add_library(CrashRptProbe SHARED ${source_files} ${header_files})
else(CRASHRPT_BUILD_SHARED_LIBS)
  add_library(CrashRptProbe STATIC ${source_files} ${header_files})
endif(CRASHRPT_BUILD_SHARED_LIBS)

if(CMAKE_CL_64)
  link_directories( ${CRASHRPT_SRC}/thirdparty/dbghelp/lib/amd64 )
  target_link_libraries(CrashRptProbe ${CRASHRPT_SRC}/thirdparty/dbghelp/lib/amd64/dbghelp.lib)
else(CMAKE_CL_64)
  link_directories( ${CRASHRPT_SRC}/thirdparty/dbghelp/lib )
  target_link_libraries(CrashRptProbe ${CRASHRPT_SRC}/thirdparty/dbghelp/lib/dbghelp.lib)
endif(CMAKE_CL_64)

target_link_libraries(CrashRptProbe zlib minizip tinyxml Rpcrt4.lib shell32.lib gdi32.lib version.lib psapi.lib)

if(CRASHRPT_BUILD_SHARED_LIBS)
  set_target_properties(CrashRptProbe PROPERTIES
                  DEBUG_POSTFIX ${CRASHRPT_VER}d
                  RELEASE_POSTFIX ${CRASHRPT_VER}
                  RELWITHDEBINFO_POSTFIX ${CRASHRPT_VER}
                  MINSIZEREL_POSTFIX ${CRASHRPT_VER})
else(CRASHRPT_BUILD_SHARED_LIBS)
  set_target_properties(CrashRptProbe PROPERTIES
                  DEBUG_POSTFIX LIBd
                  RELEASE_POSTFIX LIB
                  RELWITHDEBINFO_POSTFIX LIB
                  MINSIZEREL_POSTFIX LIB)
endif(CRASHRPT_BUILD_SHARED_LIBS)

INSTALL(TARGETS CrashRptProbe
             LIBRARY DESTINATION ${CRASHRPT_INSTALLDIR_BIN}
             ARCHIVE DESTINATION ${CRASHRPT_INSTALLDIR_LIB}
             RUNTIME DESTINATION ${CRASHRPT_INSTALLDIR_BIN})

install(FILES $<TARGET_PDB_FILE:CrashRptProbe> DESTINATION ${CRASHRPT_INSTALLDIR_BIN})
