SUBDIRS = Confuse sdl textscreen

gamesdir = /usr/local/games

games_PROGRAMS = eternity

AM_CXXFLAGS = -I/usr/include/SDL -I/usr/local/include/SDL @SDL_CFLAGS@ -Wall\
-D_SDL_VER -DNDEBUG -DAMX_NODYNALOAD -DR_LINKEDPORTALS -DZONE_NATIVE \
-DHAVE_SCHED_SETAFFINITY -D_CONSOLE -DLINUX -fno-strict-aliasing

eternity_SOURCES = \
a_common.cpp   a_common.h     a_counters.cpp a_decorate.cpp a_doom.cpp     a_doom.h \
a_fixed.cpp    a_general.cpp  a_heretic.cpp  a_hexen.cpp    a_small.cpp    a_small.h \
acs_intr.cpp   acs_intr.h \
am_color.cpp   am_map.cpp     am_map.h \
amx.cpp        amx.h          amxcore.cpp \
c_cmd.cpp      c_io.cpp       c_io.h         c_net.cpp      c_net.h        c_runcmd.cpp \
c_runcmd.h \
Confuse \
d_deh.cpp      d_deh.h        d_dehtbl.cpp   d_dehtbl.h     d_diskfile.cpp d_diskfile.h \
d_dwfile.h     d_englsh.h     d_event.h      d_french.h     d_gi.cpp       d_gi.h \
dhticstr.h     d_io.cpp       d_io.h         d_items.cpp    d_items.h      d_keywds.h \
d_main.cpp     d_main.h       d_mod.h        d_net.cpp      d_net.h        doomdata.h \
doomdef.cpp    doomdef.h      doomstat.cpp   doomstat.h     doomtype.h     d_player.h \
dstrings.cpp   dstrings.h     d_textur.h     d_think.h      d_ticcmd.h \
e_args.cpp     e_args.h       e_cmd.cpp      e_dstate.cpp   e_dstate.h     e_edf.cpp \
e_edf.h        e_exdata.cpp   e_exdata.h     e_fonts.cpp    e_fonts.h      e_hash.cpp \
e_hash.h       e_lib.cpp      e_lib.h        e_mod.cpp      e_mod.h        e_player.cpp \
e_player.h     e_sound.cpp    e_sound.h      e_sprite.cpp   e_sprite.h     e_states.cpp \
e_states.h     e_string.cpp   e_string.h     e_things.cpp   e_things.h     e_ttypes.cpp \
e_ttypes.h \
f_finale.cpp   f_finale.h     f_wipe.cpp     f_wipe.h \
g_bind.cpp     g_bind.h       g_cmd.cpp      g_dmflag.cpp   g_dmflag.h     g_game.cpp \
g_game.h       g_gfs.cpp      g_gfs.h \
hi_stuff.cpp   hi_stuff.h     hu_frags.cpp   hu_frags.h     hu_over.cpp    hu_over.h \
hu_stuff.cpp   hu_stuff.h \
i_net.h        info.cpp       info.h         in_lude.cpp    in_lude.h      i_sound.h \
i_system.h     i_video.h \
m_argv.cpp     m_argv.h       m_bbox.cpp     m_bbox.h       m_buffer.cpp   m_buffer.h \
m_cheat.cpp    m_cheat.h      m_dllist.h     m_fcvt.cpp     m_fcvt.h       m_fixed.h \
m_hash.cpp     m_hash.h       m_misc.cpp     m_misc.h       m_qstr.cpp     m_qstr.h \
m_queue.cpp    m_queue.h      m_random.cpp   m_random.h     m_shots.cpp    m_shots.h \
m_swap.h       m_syscfg.cpp   m_syscfg.h     m_vector.cpp   m_vector.h \
mn_emenu.cpp   mn_emenu.h     mn_engin.cpp   mn_engin.h     mn_files.cpp   mn_files.h \
mn_htic.cpp    mn_htic.h      mn_menus.cpp   mn_menus.h     mn_misc.cpp    mn_misc.h \
mn_skinv.cpp \
metaapi.cpp    metaapi.h \
osdefs.h \
p_anim.cpp     p_anim.h       p_ceilng.cpp   p_chase.cpp    p_chase.h      p_cmd.cpp \
p_doors.cpp    p_enemy.cpp    p_enemy.h      p_floor.cpp    p_genlin.cpp   p_hubs.cpp \
p_hubs.h       p_info.cpp     p_info.h       p_inter.cpp    p_inter.h      p_lights.cpp \
p_map.cpp      p_map.h        p_map3d.cpp    p_map3d.h      p_maputl.cpp   p_maputl.h \
p_mobj.cpp     p_mobj.h       p_partcl.cpp   p_partcl.h     p_plats.cpp    p_portal.cpp \
p_portal.h     p_pspr.cpp     p_pspr.h       p_saveg.cpp    p_saveg.h      p_setup.cpp \
p_setup.h      p_sight.cpp    p_skin.cpp     p_skin.h       p_slopes.cpp   p_slopes.h \
p_spec.cpp     p_spec.h       p_switch.cpp   p_telept.cpp   p_things.cpp   p_tick.cpp \
p_tick.h       p_trace.cpp    p_user.cpp     p_user.h       p_xenemy.cpp   p_xenemy.h \
polyobj.cpp    polyobj.h      psnprntf.cpp   psnprntf.h \
r_bsp.cpp      r_bsp.h        r_data.cpp     r_data.h       r_defs.h       r_draw.cpp \
r_draw.h       r_drawl.cpp    r_drawl.h      r_drawq.cpp    r_dynseg.h     r_main.cpp \
r_main.h       r_plane.cpp    r_pcheck.h     r_drawq.h      r_dynseg.cpp   r_plane.h \
r_portal.cpp   r_portal.h     r_ripple.cpp   r_ripple.h     r_segs.cpp     r_segs.h \
r_sky.cpp      r_sky.h        r_span.cpp     r_state.h      r_textur.cpp   r_things.cpp \
r_things.h     rules.make \
s_sndseq.cpp   s_sndseq.h     s_sound.cpp    s_sound.h \
sclinux.h      sounds.cpp     sounds.h \
st_hbar.cpp    st_lib.cpp     st_lib.h       st_stuff.cpp   st_stuff.h \
tables.cpp     tables.h \
v_block.cpp    v_block.h      v_buffer.cpp   v_buffer.h     v_font.cpp     v_font.h \
v_misc.cpp     v_misc.h       v_patch.cpp    v_patch.h      v_video.cpp    v_video.h \
version.cpp    version.h \
wi_stuff.cpp   wi_stuff.h \
w_levels.cpp   w_levels.h     w_wad.cpp      w_wad.h \
xl_scripts.cpp xl_scripts.h \
z_native.cpp   z_zone.h

eternity_LDADD = @LDFLAGS@ @SDL_LIBS@ -lSDL_mixer -lSDL_net sdl/libsdlet.a\
Confuse/libetconfuse.a textscreen/libtextscreen.a
