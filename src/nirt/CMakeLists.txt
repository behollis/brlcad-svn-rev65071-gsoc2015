set(NIRT_INCLUDE_DIRS
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${SYSV_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES NIRT_INCLUDE_DIRS)
include_directories(${NIRT_INCLUDE_DIRS})

set(NIRT_SRCS
  nirt.c
  command.c
  conversion.c
  if.c
  interact.c
  bsphere.c
  read_mat.c
  str_manip.c
  parse_fmt.c
  )

BRLCAD_ADDEXEC(nirt "${NIRT_SRCS}" "librt;libfb;libsysv;${M_LIBRARY}")
SET_PROPERTY(TARGET nirt APPEND PROPERTY COMPILE_DEFINITIONS "DFLT_SHELL=\"/bin/sh\"")

BRLCAD_ADDEXEC(showshot showshot.c libbn)

set(nirt_sfiles
  sfiles/csv.nrt
  sfiles/csv-gap.nrt
  sfiles/default.nrt
  sfiles/entryexit.nrt
  sfiles/gap1.nrt
  sfiles/gap2.nrt
  )
BRLCAD_ADDDATA(nirt_sfiles nirt)

set(nirt_ignore_files
  nirt.h
  usrfmt.h
  )
CMAKEFILES(${nirt_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
