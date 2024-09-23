project(crprober)

# source files
aux_source_directory( . SOURCE_FILE_LIST )
file( GLOB HEADER_FILE_LIST *.h )

# unicode
add_compile_definitions( _UNICODE UNICODE )

# include paths
cmake_path(SET REPOSITORY_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include_directories(${REPOSITORY_DIR}/include)

# exe
add_executable(crprober ${SOURCE_FILE_LIST} ${HEADER_FILE_LIST})

# link
target_link_libraries(crprober CrashRptProbe)

# add "d" postfix to output file name in debug
set_target_properties(crprober PROPERTIES DEBUG_POSTFIX d )