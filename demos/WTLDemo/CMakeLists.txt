project(WTLDemo)

# source files
aux_source_directory(. SOURCE_FILE_LIST)
file(GLOB HEADER_FILE_LIST *.h)

# resource files
list(APPEND SOURCE_FILE_LIST ./WTLDemo.rc )

# unicode
add_compile_definitions(UNICODE _UNICODE)

# include paths
cmake_path(SET REPOSITORY_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include_directories(
  ${REPOSITORY_DIR}/include
  ${REPOSITORY_DIR}/thirdparty/wtl)

# target
add_executable(WTLDemo WIN32 ${SOURCE_FILE_LIST} ${HEADER_FILE_LIST})

# pch
target_precompile_headers(WTLDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h)

# link
target_link_libraries(WTLDemo CrashRpt)

# add "d" postfix to output file name in debug
set_target_properties(WTLDemo PROPERTIES DEBUG_POSTFIX d )
set_target_properties(WTLDemo PROPERTIES COMPILE_FLAGS "/Od /Zi /fp:except" LINK_FLAGS "/DEBUG")

# Add post-build event
if(CMAKE_CL_64)
set(AMD_SUFFIX "amd64")
endif()

# command
add_custom_command(
    TARGET WTLDemo
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} ARGS -E copy "\"${REPOSITORY_DIR}/demos/WTLDemo/dummy.ini\" \"${CMAKE_BINARY_DIR}/bin/\""
    COMMAND ${CMAKE_COMMAND} ARGS -E copy "\"${REPOSITORY_DIR}/demos/WTLDemo/dummy.log\" \"${CMAKE_BINARY_DIR}/bin/\""
    COMMAND ${CMAKE_COMMAND} ARGS -E copy "\"${REPOSITORY_DIR}/lang_files\\crashrpt_lang_EN.ini\" \"${CMAKE_BINARY_DIR}/bin/crashrpt_lang.ini\"")

if(NOT DBGHELP_USE_INSTALLED_SDK)
  add_custom_command(
    TARGET WTLDemo
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} ARGS -E copy "\"${REPOSITORY_DIR}/thirdparty/dbghelp/bin/${AMD_SUFFIX}/dbghelp.dll\" \"${CMAKE_BINARY_DIR}/bin/\"")
endif()
