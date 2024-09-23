project(libpng)

# Create the list of source files
aux_source_directory(. SOURCE_FILE_LIST)

# unicode
add_compile_definitions(_UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)

add_compile_options(/wd4206 /wd4245 /wd4267 /wd4310)

# include paths
cmake_path(SET REPOSITORY_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include_directories(${REPOSITORY_DIR}/thirdparty/zlib)

# target
add_library(libpng STATIC ${SOURCE_FILE_LIST})

# link
target_link_libraries(libpng zlib)

set_target_properties(libpng PROPERTIES DEBUG_POSTFIX d)
