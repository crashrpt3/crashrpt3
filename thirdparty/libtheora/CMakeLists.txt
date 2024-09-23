project(libtheora)

# source files
aux_source_directory( ./lib SOURCE_FILE_LIST )
aux_source_directory( ./lib/x86_vc X86_SOURCE_FILE_LIST )

# unicode
add_compile_definitions(UNICODE _UNICODE)

add_compile_options( /wd4018 /wd4100 /wd4132 /wd4214 /wd4244 /wd4245 /wd4267 /wd4334 /wd4389 /wd4554 /wd4700 /wd4701 )

# include paths
cmake_path(SET REPOSITORY_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${REPOSITORY_DIR}/thirdparty/libogg/include)

foreach (flag_var CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
	string(REPLACE "/RTC1" "-RTCs" ${flag_var} "${${flag_var}}")
endforeach()

# target
add_library(libtheora STATIC ${SOURCE_FILE_LIST} ${X86_SOURCE_FILE_LIST})

# add "d" postfix to output file name in debug
set_target_properties(libtheora PROPERTIES DEBUG_POSTFIX d)
