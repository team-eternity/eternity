SUBDIRS = Confuse sdl textscreen

gamesdir = /usr/local/games

games_PROGRAMS = eternity

AM_CFLAGS = -I/usr/include/SDL -I/usr/local/include/SDL @SDL_CFLAGS@ -Wall\
-D_SDL_VER -DNDEBUG -DAMX_NODYNALOAD -DR_LINKEDPORTALS -DR_PORTALS     \
-DOVER_UNDER -DPOLYOBJECTS -D_CONSOLE -DLINUX -fno-strict-aliasing

eternity_SOURCES = \
acs_intr.c  acs_intr.h  a_fixed.c   am_color.c  am_map.c    am_map.h    amx.c       \
amxcore.c   amx.h       a_small.c   a_small.h   c_cmd.c     c_io.c      c_io.h      \
c_net.c     c_net.h     Confuse     c_runcmd.c  c_runcmd.h  d_deh.c     d_deh.h     \
d_dehtbl.c  d_dehtbl.h  d_dwfile.h  d_englsh.h  d_event.h   d_french.h  d_gi.c      \
d_gi.h      dhticstr.h  d_io.c      d_io.h      d_items.c   d_items.h   d_keywds.h  \
d_main.c    d_main.h    e_player.c  e_player.h  d_mod.h     d_net.c     d_net.h     \
doomdata.h  doomdef.c   doomdef.h   doomstat.c  doomstat.h  doomtype.h  d_player.h  \
dstrings.c  dstrings.h  d_textur.h  d_think.h   d_ticcmd.h  e_cmd.c     e_edf.c     \
e_edf.h     e_exdata.c  e_exdata.h  e_fonts.c   e_fonts.h   e_lib.c     e_lib.h     \
e_sound.c   e_sound.h   e_states.c  e_states.h  e_string.c  e_string.h  e_things.c  \
e_things.h  e_ttypes.c  e_ttypes.h  f_finale.c  f_finale.h  f_wipe.c    f_wipe.h    \
g_bind.c    g_bind.h    g_cmd.c     g_dmflag.c  g_dmflag.h  g_game.c    g_game.h    \
g_gfs.c     g_gfs.h     hi_stuff.c  hi_stuff.h  hu_frags.c  hu_frags.h  hu_over.c   \
hu_over.h   hu_stuff.c  hu_stuff.h  i_net.h     info.c      info.h      in_lude.c   \
in_lude.h   i_sound.h   i_system.h  i_video.h   m_argv.c    m_argv.h    m_bbox.c    \
m_bbox.h    m_cheat.c   p_xenemy.c  p_xenemy.h  e_mod.c     e_mod.h     m_cheat.h   \
m_dllist.h  m_fcvt.c    m_fcvt.h    m_fixed.h   m_misc.c    m_misc.h    mn_emenu.c  \
mn_emenu.h  mn_engin.c  mn_engin.h  mn_files.c  mn_files.h  mn_htic.c   mn_htic.h   \
mn_menus.c  mn_menus.h  mn_misc.c   mn_misc.h   mn_skinv.c  m_qstr.c    m_qstr.h    \
m_queue.c   m_queue.h   m_random.c  m_random.h  m_swap.h    m_syscfg.c  m_syscfg.h  \
osdefs.h    p_anim.c    p_anim.h    p_ceilng.c  p_chase.c   p_chase.h   p_cmd.c     \
p_doors.c   p_enemy.c   p_enemy.h   p_floor.c   p_genlin.c  p_henemy.c  p_hubs.c    \
p_hubs.h    p_info.c    p_info.h    p_inter.c   p_inter.h   p_lights.c  p_map.c     \
p_map.h     p_map3d.c   p_map3d.h   p_maputl.c  p_maputl.h  p_mobj.c    p_mobj.h    \
polyobj.c   polyobj.h   p_partcl.c  p_partcl.h  p_plats.c   p_portal.c  p_portal.h  \
p_pspr.c    p_pspr.h    p_saveg.c   p_saveg.h   p_setup.c   p_setup.h   p_sight.c   \
p_skin.c    p_skin.h    p_slopes.c  p_slopes.h  psnprntf.c  psnprntf.h  p_spec.c    \
p_spec.h    p_switch.c  p_telept.c  p_tick.c    p_tick.h    p_trace.c   p_user.c    \
p_user.h    r_bsp.c     r_bsp.h     r_data.c    r_data.h    r_defs.h    r_draw.c    \
r_draw.h    r_drawl.c   r_drawl.h   r_drawq.c   r_dynseg.h  r_main.c    r_main.h    \
r_plane.c   r_pcheck.h  r_drawq.h   r_dynseg.c  r_plane.h   r_portal.c  r_portal.h  \
r_ripple.c  r_ripple.h  r_segs.c    r_segs.h    r_sky.c     r_sky.h     r_span.c    \
r_state.h   r_things.c  r_things.h  rules.make  sclinux.h   sounds.c    sounds.h    \
s_sndseq.c  s_sndseq.h  s_sound.c   s_sound.h   st_hbar.c   st_lib.c    st_lib.h    \
st_stuff.c  st_stuff.h  tables.c    tables.h    v_block.c   v_block.h   v_buffer.c  \
v_buffer.h  version.c   version.h   v_font.c    v_font.h    v_misc.c    v_misc.h    \
v_patch.c   v_patch.h   v_video.c   v_video.h   wi_stuff.c  wi_stuff.h  w_wad.c     \
w_wad.h     z_zone.c    z_zone.h

eternity_LDADD = @LDFLAGS@ @SDL_LIBS@ -lSDL_mixer -lSDL_net sdl/libsdlet.a\
Confuse/libetconfuse.a textscreen/libtextscreen.a
