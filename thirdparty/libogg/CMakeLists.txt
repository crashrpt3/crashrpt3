project(libogg)

# source files
aux_source_directory(./src SOURCE_FILE_LIST)

# unicode
add_compile_definitions(_UNICODE UNICODE)

# include paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# target
add_library(libogg STATIC ${SOURCE_FILE_LIST})

# add "d" postfix to output file name in debug
set_target_properties(libogg PROPERTIES DEBUG_POSTFIX d)