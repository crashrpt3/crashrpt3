project(ConsoleDemo)

# source files
aux_source_directory(. SOURCE_FILE_LIST)
file(GLOB HEADER_FILE_LIST *.h)

# exe
add_executable(ConsoleDemo ${SOURCE_FILE_LIST} ${HEADER_FILE_LIST})

# include paths
cmake_path(SET REPOSITORY_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include_directories(${REPOSITORY_DIR}/include)

# unicode
add_compile_definitions(UNICODE _UNICODE)

# link
target_link_libraries(ConsoleDemo CrashRpt)

# add "d" postfix to output file name in debug
set_target_properties(ConsoleDemo PROPERTIES DEBUG_POSTFIX d)