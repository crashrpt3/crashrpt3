project(crashrpt)

# Create the list of source files
aux_source_directory( . source_files )
file( GLOB header_files *.h ../../include/CrashRpt.h )

list(APPEND source_files ./CrashRpt.def)

# Enable usage of precompiled header
set(srcs_using_precomp ${source_files})
list(REMOVE_ITEM srcs_using_precomp ./StdAfx.cpp ./CrashRpt.def)
add_msvc_precompiled_header(stdafx.h ./StdAfx.cpp srcs_using_precomp)

# Define _UNICODE and UNICODE (use wide-char encoding)
add_compile_definitions( _UNICODE UNICODE NOMINMAX)

# Add include dir
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  )

# Add library build target
if(CRASHRPT_BUILD_SHARED_LIBS)
  add_library(crashrpt SHARED ${source_files} ${header_files})
endif()

# Specify what libraries to link
target_link_libraries(crashrpt rpcrt4.dll version.lib yyjson)

if(CRASHRPT_BUILD_SHARED_LIBS)
  set_target_properties(crashrpt PROPERTIES
    DEBUG_POSTFIX ${CRASHRPT_VER}d
    RELEASE_POSTFIX ${CRASHRPT_VER}
    RELWITHDEBINFO_POSTFIX ${CRASHRPT_VER}
    MINSIZEREL_POSTFIX ${CRASHRPT_VER}
  )
endif()
