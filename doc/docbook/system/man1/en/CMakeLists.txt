set(docbook_man1_EN
  anim_script.xml
  anim_sort.xml
  anim_time.xml
  anim_track.xml
  anim_turn.xml
  ap-pix.xml
  asc-nmg.xml
  asc-plot3.xml
  asc2dsp.xml
  asc2g.xml
  azel.xml
  bary.xml
  benchmark.xml
  bot_dump.xml
  bot_shell-vtk.xml
  brep_simple.xml
  brlcad-config.xml
  brlcad.xml
  buffer.xml
  bw-fb.xml
  bw-imp.xml
  bw-pix.xml
  bw-png.xml
  bw-ps.xml
  bw-rle.xml
  bw3-pix.xml
  bwcrop.xml
  bwdiff.xml
  bwfilter.xml
  bwhist.xml
  bwhisteq.xml
  bwmod.xml
  bwrect.xml
  bwrot.xml
  bwscale.xml
  bwshrink.xml
  bwstat.xml
  bwthresh.xml
  cad_boundp.xml
  cad_parea.xml
  canonize.xml
  cell-fb.xml
  chan_mult.xml
  chan_permute.xml
  cmap-fb.xml
  coil.xml
  comgeom-g.xml
  conv-vg2g.xml
  cv.xml
  cy-g.xml
  db.xml
  dbclean.xml
  dbcp.xml
  dbupgrade.xml
  decimate.xml
  dpix-pix.xml
  dxf-g.xml
  enf-g.xml
  euclid-g.xml
  fast4-g.xml
  fb-bw.xml
  fb-cmap.xml
  fb-fb.xml
  fb-pix.xml
  fb-png.xml
  fb-rle.xml
  fbanim.xml
  fbcbars.xml
  fbclear.xml
  fbcmap.xml
  fbcmrot.xml
  fbcolor.xml
  fbed.xml
  fbfade.xml
  fbframe.xml
  fbfree.xml
  fbgamma.xml
  fbgrid.xml
  fbhelp.xml
  fblabel.xml
  fbline.xml
  fbpoint.xml
  fbscanplot.xml
  fbserv.xml
  fbstretch.xml
  fbzoom.xml
  firpass.xml
  g-acad.xml
  g-dxf.xml
  g-egg.xml
  g-euclid.xml
  g-iges.xml
  g-nff.xml
  g-nmg.xml
  g-obj.xml
  g-raw.xml
  g-shell-rect.xml
  g-step.xml
  g-stl.xml
  g-tankill.xml
  g-var.xml
  g-voxel.xml
  g-vrml.xml
  g-x3d.xml
  g-dot.xml
  g2asc.xml
  g_diff.xml
  g_lint.xml
  gdiff.xml
  gencolor.xml
  gif-fb.xml
  gif2fb.xml
  gqa.xml
  halftone.xml
  hex.xml
  iges-g.xml
  imgdims.xml
  ir-X.xml
  irdisp.xml
  lgt.xml
  loop.xml
  mac-pix.xml
  mged.xml
  morphedit.xml
  nastran-g.xml
  nirt.xml
  nmg-bot.xml
  nmg-rib.xml
  obj-g.xml
  patch-g.xml
  pictx.xml
  pix-alias.xml
  pix-bw.xml
  pix-bw3.xml
  pix-fb.xml
  pix-png.xml
  pix-ppm.xml
  pix-ps.xml
  pix-rle.xml
  pix-sun.xml
  pix2asc.xml
  pixautosize.xml
  pixbackgnd.xml
  pixbgstrip.xml
  pixblend.xml
  pixborder.xml
  pixbustup.xml
  pixclump.xml
  pixcmp.xml
  pixcolors.xml
  pixcut.xml
  pixdiff.xml
  pixdsplit.xml
  pixfade.xml
  pixfilter.xml
  pixhalve.xml
  pixhist.xml
  pixhist3d-plot3.xml
  pixhist3d.xml
  pixinterp2x.xml
  pixmatte.xml
  pixmerge.xml
  pixmorph.xml
  pixrect.xml
  pixrot.xml
  pixsaturate.xml
  pixscale.xml
  pixshrink.xml
  pixstat.xml
  pixsubst.xml
  pixtile.xml
  pixuntile.xml
  plot3-X.xml
  plot3-asc.xml
  plot3-fb.xml
  plot3-hpgl.xml
  plot3-plot3.xml
  plot3-ps.xml
  plot3-tek.xml
  plot3color.xml
  plot3debug.xml
  plot3getframe.xml
  plot3line2.xml
  plot3rot.xml
  ply-g.xml
  png-bw.xml
  png-fb.xml
  png-pix.xml
  png_info.xml
  polar-fb.xml
  poly-bot.xml
  pp-fb.xml
  proe-g.xml
  random.xml
  remapid.xml
  remrt.xml
  reshoot.xml
  rle-fb.xml
  rle-pix.xml
  rpatch.xml
  rt.xml
  rt_bot_faces.xml
  rtarea.xml
  rtcheck.xml
  rtedge.xml
  rtexample.xml
  rtg3.xml
  rthide.xml
  rtpp.xml
  rtrange.xml
  rtray.xml
  rtregis.xml
  rtscale.xml
  rtshot.xml
  rttherm.xml
  rtwalk.xml
  rtweight.xml
  rtwizard.xml
  rtxray.xml
  scriptsort.xml
  secpass.xml
  shapefact.xml
  showshot.xml
  showtherm.xml
  ssamp-bw.xml
  stl-g.xml
  sun-pix.xml
  tabinterp.xml
  tabsub.xml
  tankill-g.xml
  terrain.xml
  tire.xml
  ug-g.xml
  vdeck.xml
  viewpoint-g.xml
  wavelet.xml
  wdb_example.xml
  )

# Some DocBook man pages generate multiple output files, if they
# describe more than one command.  Those files need special
# handling, since the CMake logic has no way to know about the
# extra outputs without being told.
set_source_files_properties(pix2asc.xml PROPERTIES EXTRA_MAN_OUTPUTS "asc2pix.1")
set_source_files_properties(chan_mult.xml PROPERTIES EXTRA_MAN_OUTPUTS "chan_add.1")

DOCBOOK_TO_HTML(man1 docbook_man1_EN html/man1/en "")
DOCBOOK_TO_MAN(man1 docbook_man1_EN 1 1 man1 "")

if(BRLCAD_EXTRADOCS_PDF_MAN)
  DOCBOOK_TO_PDF(man1 docbook_man1_EN pdf/man1/en "")
endif(BRLCAD_EXTRADOCS_PDF_MAN)

CMAKEFILES(pictx.xml)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
