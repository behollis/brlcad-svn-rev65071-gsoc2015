set(FB_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR_GL}
  ${OPENGL_INCLUDE_DIR_GLX}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
  ${OPENNURBS_INCLUDE_DIR}
  ${Qt5Widgets_INCLUDE_DIRS}
  ${OPENTHREADS_INCLUDE_DIR}
  ${OSG_INCLUDE_DIR}
  )
BRLCAD_LIB_INCLUDE_DIRS(fb FB_INCLUDE_DIRS "")

set(LINKLIBS "")
if(BRLCAD_ENABLE_X11 AND X11_FOUND)
  set(FBX11DEFS "-DIF_X")
  set(LINKLIBS ${LINKLIBS} ${X11_LIBRARIES})
  set(fbX11_srcs if_X24.c)
  set_property(SOURCE if_X24.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif()
if(BRLCAD_ENABLE_OPENGL AND OPENGL_FOUND AND NOT MSVC)
  set(FBOGLDEFS "-DIF_OGL")
  set(LINKLIBS ${LINKLIBS} ${OPENGL_LIBRARIES})
  set(fbogl_srcs if_ogl.c)
  set_property(SOURCE if_ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif()
if(WIN32)
  add_definitions(
    -DIF_WGL
    )
  set(LINKLIBS ${LINKLIBS} opengl32.lib ${TCL_TK_LIBRARY})
  set(fbwgl_srcs if_wgl.c)
  set_property(SOURCE if_wgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(WIN32)
if(BRLCAD_ENABLE_TK)
  set(FBTKDEFS "-DIF_TK")
  set(LINKLIBS ${LINKLIBS} ${TK_LIBRARY})
  set(fbtk_srcs if_tk.c)
  set_property(SOURCE if_tk.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(BRLCAD_ENABLE_TK)
if(BRLCAD_ENABLE_QT)
  CHECK_CXX_FLAG(Wno-float-equal)

  set(FBQTDEFS "-DIF_QT")
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  add_definitions(${Qt5Widgets_DEFINITIONS})
  set(LINKLIBS ${LINKLIBS} ${Qt5Widgets_LIBRARIES})
  set(fbqt_srcs if_qt.cpp)
  set_property(SOURCE if_qt.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(BRLCAD_ENABLE_QT)

if(BRLCAD_ENABLE_OSG)
  set(FBOSGLDEFS "-DIF_OSGL")
  set(OSG_LIBRARIES
    ${OSG_LIBRARY}
    ${OSGTEXT_LIBRARY}
    ${OSGVIEWER_LIBRARY}
    )
  set(LINKLIBS ${LINKLIBS} ${OSG_LIBRARIES})
  set(fbosgl_srcs if_osgl.cpp)
  set_property(SOURCE if_osgl.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif()

set(LIBFB_SOURCES
  ${fbX11_srcs}
  ${fbogl_srcs}
  ${fbwgl_srcs}
  ${fbtk_srcs}
  ${fbqt_srcs}
  ${fbosgl_srcs}
  asize.c
  fb_generic.c
  fb_log.c
  fb_obj.c
  fb_paged_io.c
  fb_rect.c
  fb_util.c
  fbserv_obj.c
  if_debug.c
  if_disk.c
  if_mem.c
  if_null.c
  if_remote.c
  if_stack.c
  tcl.c
  vers.c
  )

add_definitions(
  -DIF_REMOTE
  -DIF_AB
  ${FBX11DEFS}
  ${FBOGLDEFS}
  ${FBTKDEFS}
  ${XOPEN_DEFINE}
  ${FBQTDEFS}
  ${FBOSGLDEFS}
  )

BRLCAD_ADDLIB(libfb "${LIBFB_SOURCES}" "libbu;libpkg;${LINKLIBS}")
SET_TARGET_PROPERTIES(libfb PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
  ADD_DEPENDENCIES(libfb tk)
endif(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)

set(libfb_ignore_files
  if_TEMPLATE.c
  if_X24.c
  if_ogl.c
  if_osgl.cpp
  if_qt.cpp
  if_tk.c
  if_wgl.c
  fb_private.h
  osg_fb_manipulator.h
  )
CMAKEFILES(${libfb_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
