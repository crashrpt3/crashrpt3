project(minizip)

# source files
aux_source_directory( . SOURCE_FILE_LIST )

# unicode
add_compile_definitions(UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)

add_compile_options( /wd4100 /wd4131 /wd4189 /wd4244 )

# include paths
cmake_path(SET REPOSITORY_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include_directories(${REPOSITORY_DIR}/thirdparty/zlib)

# target
add_library(minizip STATIC ${SOURCE_FILE_LIST})

# link
target_link_libraries(minizip zlib)

# add "d" postfix to output file name in debug
set_target_properties(minizip PROPERTIES DEBUG_POSTFIX d )
