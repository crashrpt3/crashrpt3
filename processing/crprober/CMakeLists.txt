project(crprober)

# Create the list of source files
aux_source_directory( . source_files )
file( GLOB header_files *.h )

# Define _UNICODE and UNICODE (use wide-char encoding)
add_compile_definitions( _UNICODE UNICODE )

fix_default_compiler_settings_()

# Add include dir
include_directories(${CRASHRPT_SRC}/include)

# Add executable build target
add_executable(crprober ${source_files} ${header_files})

# Add input link libraries
target_link_libraries(crprober CrashRptProbe)

set_target_properties(crprober PROPERTIES DEBUG_POSTFIX d )

INSTALL(TARGETS crprober
  LIBRARY DESTINATION ${CRASHRPT_INSTALLDIR_BIN}
  ARCHIVE DESTINATION ${CRASHRPT_INSTALLDIR_LIB}
  RUNTIME DESTINATION ${CRASHRPT_INSTALLDIR_BIN}
)
