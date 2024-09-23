project(Tests)

# Create the list of source files
aux_source_directory( . SOURCE_FILE_LIST )
file( GLOB HEADER_FILE_LIST *.h )

# repository directory
cmake_path(SET REPOSITORY_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/..")

list(APPEND SOURCE_FILE_LIST ${REPOSITORY_DIR}/reporting/CrashRpt/Utility.cpp)

# Enable usage of precompiled header
set(srcs_using_precomp ${SOURCE_FILE_LIST})
list(REMOVE_ITEM srcs_using_precomp ./stdafx.cpp)
add_msvc_precompiled_header(stdafx.h ./stdafx.cpp srcs_using_precomp)

# Define _UNICODE and UNICODE (use wide-char encoding)
add_compile_definitions(_UNICODE UNICODE)

add_compile_options( /wd4100 )

if(NOT CRASHRPT_BUILD_SHARED_LIBS)
  add_compile_definitions(CRASHRPT_LIB)
endif(NOT CRASHRPT_BUILD_SHARED_LIBS)

set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")

# Add include dir
include_directories(
  ${REPOSITORY_DIR}/include
  ${REPOSITORY_DIR}/reporting/CrashRpt
  ${REPOSITORY_DIR}/thirdparty/wtl
)

# Add executable build target
add_executable(Tests ${SOURCE_FILE_LIST} ${HEADER_FILE_LIST})

# Add input link libraries
target_link_libraries(Tests CrashRpt CrashRptProbe)

set_target_properties(Tests PROPERTIES DEBUG_POSTFIX d )

add_dependencies(Tests CrashRpt CrashRptProbe CrashSender crprober)