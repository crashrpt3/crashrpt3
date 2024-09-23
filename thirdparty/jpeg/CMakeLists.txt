project(libjpeg)

# source files
aux_source_directory(. SOURCE_FILE_LIST)

# unicode
add_compile_definitions(_UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)

add_compile_options(/wd4100 /wd4127)

# lib
add_library(libjpeg STATIC ${SOURCE_FILE_LIST})

# add "d" postfix to output file name in debug
set_target_properties(libjpeg PROPERTIES DEBUG_POSTFIX d)
