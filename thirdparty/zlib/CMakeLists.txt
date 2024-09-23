project(zlib)

aux_source_directory( . SOURCE_FILE_LIST )

# unicode
add_compile_definitions(UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)

add_compile_options(/wd4127 /wd4131 /wd4244 /wd4245 /wd4267 /wd4996)

# lib
add_library(zlib STATIC ${SOURCE_FILE_LIST})

# add "d" postfix to output file name in debug
set_target_properties(zlib PROPERTIES DEBUG_POSTFIX d)
