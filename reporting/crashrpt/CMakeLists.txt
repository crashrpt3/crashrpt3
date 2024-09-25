project(CrashRpt)

# Create the list of source files
aux_source_directory( . source_files )
file( GLOB header_files *.h ../../include/CrashRpt.h )

list(APPEND source_files ./CrashRpt.rc ./CrashRpt.def)

# Enable usage of precompiled header
set(srcs_using_precomp ${source_files})
list(REMOVE_ITEM srcs_using_precomp ./CrashRpt.rc ./StdAfx.cpp ./CrashRpt.def)
add_msvc_precompiled_header(stdafx.h ./StdAfx.cpp srcs_using_precomp)

# Define _UNICODE and UNICODE (use wide-char encoding)
add_compile_definitions( _UNICODE UNICODE NOMINMAX)

# Add include dir
include_directories( ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/thirdparty/wtl )

# Add library build target
if(CRASHRPT_BUILD_SHARED_LIBS)
  add_library(CrashRpt SHARED ${source_files} ${header_files})
endif()

# Specify what libraries to link
target_link_libraries(CrashRpt rpcrt4.dll version.lib yyjson)

if(CRASHRPT_BUILD_SHARED_LIBS)
  set_target_properties(CrashRpt PROPERTIES
    DEBUG_POSTFIX ${CRASHRPT_VER}d
    RELEASE_POSTFIX ${CRASHRPT_VER}
    RELWITHDEBINFO_POSTFIX ${CRASHRPT_VER}
    MINSIZEREL_POSTFIX ${CRASHRPT_VER}
  )
endif()

INSTALL(TARGETS CrashRpt
  LIBRARY DESTINATION ${CRASHRPT_INSTALLDIR_BIN}
  ARCHIVE DESTINATION ${CRASHRPT_INSTALLDIR_LIB}
  RUNTIME DESTINATION ${CRASHRPT_INSTALLDIR_BIN}
)

if (CRASHRPT_INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:CrashRpt> DESTINATION ${CRASHRPT_INSTALLDIR_BIN})
endif()
