project(tinyxml)

# source files
aux_source_directory( . SOURCE_FILE_LIST )

# unicode
add_compile_definitions(UNICODE _UNICODE)

# target
add_library(tinyxml STATIC ${SOURCE_FILE_LIST})

# add "d" postfix to output file name in debug
set_target_properties(tinyxml PROPERTIES DEBUG_POSTFIX d )
