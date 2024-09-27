project(yyjson)

# source files
aux_source_directory(. SOURCE_FILE_LIST)

# unicode
add_compile_definitions(UNICODE _UNICODE)

# target
add_library(yyjson STATIC ${SOURCE_FILE_LIST})

# add "d" postfix to output file name in debug
set_target_properties(yyjson PROPERTIES DEBUG_POSTFIX d)

# include paths
target_include_directories(yyjson PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
