# Include directories needed by libbu users
set(BU_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${TCL_INCLUDE_DIRS}
  )

# locally used but not needed by users of the library
set(BU_LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(bu BU_INCLUDE_DIRS BU_LOCAL_INCLUDE_DIRS)


# Use configure_file to make copies of the mime input files,
# so CMake will get notified for the need for a re-configure
# if they are edited.
configure_file(${BRLCAD_SOURCE_DIR}/misc/mime.types ${CMAKE_CURRENT_BINARY_DIR}/tmp COPYONLY)
configure_file(${BRLCAD_SOURCE_DIR}/misc/mime_cad.types ${CMAKE_CURRENT_BINARY_DIR}/tmp COPYONLY)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/tmp)

# Set locations for final mime C and header files
set(MIME_C_FILE ${CMAKE_CURRENT_BINARY_DIR}/mime.c)
set(MIME_H_FILE ${BRLCAD_SOURCE_DIR}/include/bu/mime.h)

# Run the generation process
include(${CMAKE_CURRENT_SOURCE_DIR}/mime.cmake)

set(LIBBU_SOURCES
  affinity.c
  argv.c
  avs.c
  b64.c
  backtrace.c
  badmagic.c
  bitv.c
  bomb.c
  booleanize.c
  brlcad_path.c
  cmd.c
  cmdhist.c
  color.c
  convert.c
  crashreport.c
  ctype.c
  dirent.c
  date-time.c
  dlfcn.c
  encode.c
  endian.c
  escape.c
  fchmod.c
  fgets.c
  file.c
  fnmatch.c
  getcwd.c
  gethostname.c
  getopt.c
  globals.c
  hash.c
  heap.c
  hist.c
  hook.c
  htond.c
  htonf.c
  interrupt.c
  kill.c
  lex.c
  linebuf.c
  list.c
  log.c
  magic.c
  malloc.c
  mappedfile.c
  ${MIME_C_FILE}
  mread.c
  observer.c
  parallel.c
  parse.c
  path.c
  printb.c
  process.c
  progname.c
  ptbl.c
  rb_create.c
  rb_delete.c
  rb_diag.c
  rb_extreme.c
  rb_free.c
  rb_insert.c
  rb_order_stats.c
  rb_rotate.c
  rb_search.c
  rb_walk.c
  realpath.c
  semaphore.c
  simd.c
  sort.c
  sscanf.c
  scan.c
  str.c
  tcl.c
  temp.c
  thread.cpp
  timer.c
  units.c
  vers.c
  vfont.c
  vlb.c
  vls.c
  vls_vprintf.c
  whereis.c
  which.c
  xdr.c
  )

BRLCAD_ADDLIB(libbu "${LIBBU_SOURCES}" "${CMAKE_THREAD_LIBS_INIT};${TCL_LIBRARY};${WINSOCK_LIB};${PSAPI_LIB};${M_LIBRARY};y2038")
set_target_properties(libbu PROPERTIES VERSION 20.0.1 SOVERSION 20)
get_directory_property(BRLCAD_TCL_BUILD DIRECTORY ${BRLCAD_SOURCE_DIR}/src/other DEFINITION BRLCAD_TCL_BUILD)
if("${BRLCAD_TCL_BUILD}" STREQUAL "ON")
  add_dependencies(libbu tcl)
endif("${BRLCAD_TCL_BUILD}" STREQUAL "ON")
if(CPP_DLL_DEFINES)
  set_property(TARGET libbu APPEND PROPERTY COMPILE_DEFINITIONS "Y2038_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)

BRLCAD_ADDDATA(fix.6r vfont)
BRLCAD_ADDDATA(nonie.r.12 vfont)

add_subdirectory(tests)
add_subdirectory(y2038)

set(bu_ignore_files
  bu_internals.h
  mime.cmake
  parallel.h
  rb_internals.h
  uce-dirent.h
  vfont.h
  vls_internals.h
  )
CMAKEFILES(${bu_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
