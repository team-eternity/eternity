##
## Copyright (C) 2019 Max Waine
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <https://www.gnu.org/licenses/>.
##

eternity_args = [
   '-DR_LINKEDPORTALS',
   '-D_SDL_VER',
   '-D_CONSOLE',
   '-DHAVE_SPCLIB',
   '-DKATE_MEMORIAL',
]'

eternity_src = files(
   'acs_func.cpp',
   'acs_intr.cpp',
   'am_color.cpp',
   'am_map.cpp',
   'a_common.cpp',
   'a_counters.cpp',
   'a_decorate.cpp',
   'a_doom.cpp',
   'a_fixed.cpp',
   'a_general.cpp',
   'a_heretic.cpp',
   'a_hexen.cpp',
   'a_small.cpp',
   'a_weaponsdoom.cpp',
   'a_weaponsheretic.cpp',
   'cam_aim.cpp',
   'cam_common.cpp',
   'cam_shoot.cpp',
   'cam_sight.cpp',
   'cam_use.cpp',
   'c_batch.cpp',
   'c_cmd.cpp',
   'c_io.cpp',
   'c_net.cpp',
   'c_runcmd.cpp',
   'doomdef.cpp',
   'doomstat.cpp',
   'dstrings.cpp',
   'd_deh.cpp',
   'd_dehtbl.cpp',
   'd_diskfile.cpp',
   'd_files.cpp',
   'd_findiwads.cpp',
   'd_gi.cpp',
   'd_io.cpp',
   'd_items.cpp',
   'd_iwad.cpp',
   'd_main.cpp',
   'd_net.cpp',
   'ev_actions.cpp',
   'ev_bindings.cpp',
   'ev_sectors.cpp',
   'ev_specials.cpp',
   'ev_static.cpp',
   'e_anim.cpp',
   'e_args.cpp',
   'e_cmd.cpp',
   'e_dstate.cpp',
   'e_edf.cpp',
   'e_edfmetatable.cpp',
   'e_exdata.cpp',
   'e_fonts.cpp',
   'e_gameprops.cpp',
   'e_hash.cpp',
   'e_inventory.cpp',
   'e_lib.cpp',
   'e_mod.cpp',
   'e_player.cpp',
   'e_puff.cpp',
   'e_reverbs.cpp',
   'e_rtti.cpp',
   'e_sound.cpp',
   'e_sprite.cpp',
   'e_states.cpp',
   'e_string.cpp',
   'e_switch.cpp',
   'e_things.cpp',
   'e_ttypes.cpp',
   'e_udmf.cpp',
   'e_weapons.cpp',
   'f_finale.cpp',
   'f_wipe.cpp',
   'g_bind.cpp',
   'g_cmd.cpp',
   'g_demolog.cpp',
   'g_dmflag.cpp',
   'g_game.cpp',
   'g_gfs.cpp',
   'hi_stuff.cpp',
   'hu_boom.cpp',
   'hu_frags.cpp',
   'hu_modern.cpp',
   'hu_over.cpp',
   'hu_stuff.cpp',
   'info.cpp',
   'in_lude.cpp',
   'metaapi.cpp',
   'metaqstring.cpp',
   'mn_emenu.cpp',
   'mn_engin.cpp',
   'mn_files.cpp',
   'mn_htic.cpp',
   'mn_items.cpp',
   'mn_menus.cpp',
   'mn_misc.cpp',
   'mn_skinv.cpp',
   'm_argv.cpp',
   'm_bbox.cpp',
   'm_buffer.cpp',
   'm_cheat.cpp',
   'm_debug.cpp',
   'm_fcvt.cpp',
   'm_hash.cpp',
   'm_misc.cpp',
   'm_qstr.cpp',
   'm_queue.cpp',
   'm_random.cpp',
   'm_shots.cpp',
   'm_strcasestr.cpp',
   'm_syscfg.cpp',
   'm_utils.cpp',
   'm_vector.cpp',
   'polyobj.cpp',
   'psnprntf.cpp',
   'p_anim.cpp',
   'p_ceilng.cpp',
   'p_chase.cpp',
   'p_cmd.cpp',
   'p_doors.cpp',
   'p_enemy.cpp',
   'p_floor.cpp',
   'p_genlin.cpp',
   'p_hubs.cpp',
   'p_info.cpp',
   'p_inter.cpp',
   'p_lights.cpp',
   'p_map.cpp',
   'p_map3d.cpp',
   'p_maputl.cpp',
   'p_mobj.cpp',
   'p_mobjcol.cpp',
   'p_partcl.cpp',
   'p_plats.cpp',
   'p_portal.cpp',
   'p_portalblockmap.cpp',
   'p_portalclip.cpp',
   'p_portalcross.cpp',
   'p_pspr.cpp',
   'p_pushers.cpp',
   'p_saveg.cpp',
   'p_scroll.cpp',
   'p_sector.cpp',
   'p_setup.cpp',
   'p_sight.cpp',
   'p_skin.cpp',
   'p_slopes.cpp',
   'p_spec.cpp',
   'p_switch.cpp',
   'p_telept.cpp',
   'p_things.cpp',
   'p_tick.cpp',
   'p_trace.cpp',
   'p_user.cpp',
   'p_xenemy.cpp',
   'r_bsp.cpp',
   'r_data.cpp',
   'r_draw.cpp',
   'r_drawq.cpp',
   'r_dynabsp.cpp',
   'r_dynseg.cpp',
   'r_main.cpp',
   'r_plane.cpp',
   'r_portal.cpp',
   'r_ripple.cpp',
   'r_segs.cpp',
   'r_sky.cpp',
   'r_span.cpp',
   'r_textur.cpp',
   'r_things.cpp',
   'r_voxels.cpp',
   'sounds.cpp',
   'st_hbar.cpp',
   'st_lib.cpp',
   'st_stuff.cpp',
   's_formats.cpp',
   's_musinfo.cpp',
   's_reverb.cpp',
   's_sndseq.cpp',
   's_sound.cpp',
   'tables.cpp',
   't_plane.cpp',
   'version.cpp',
   'v_alloc.cpp',
   'v_block.cpp',
   'v_buffer.cpp',
   'v_font.cpp',
   'v_image.cpp',
   'v_misc.cpp',
   'v_patch.cpp',
   'v_patchfmt.cpp',
   'v_png.cpp',
   'v_video.cpp',
   'wi_stuff.cpp',
   'w_formats.cpp',
   'w_hacks.cpp',
   'w_levels.cpp',
   'w_wad.cpp',
   'w_zip.cpp',
   'xl_animdefs.cpp',
   'xl_emapinfo.cpp',
   'xl_mapinfo.cpp',
   'xl_musinfo.cpp',
   'xl_scripts.cpp',
   'xl_sndinfo.cpp',
   'xl_umapinfo.cpp',
   'z_native.cpp',
)

subdir('Confuse')
subdir('sdl')
subdir('textscreen')

eternity_src += [
   confuse_src,
   hal_src,
   sdl_src,
   textscreen_src,
]

# Library-specfic args & sources
if dep_opengl.found()
   subdir('gl')
   eternity_args += '-DEE_FEATURE_OPENGL'
   eternity_src  += gl_src
endif

# Build-specific args
if get_option('debug') || get_option('debugoptimized')
   eternity_args += ['-D_DEBUG', '-DRANGECHECK',
                     '-DINSTRUMENTED', '-DZONEIDCHECK']
endif

# System-specific args & sources
if build_machine.system() == 'linux'
   eternity_args += '-DLINUX'
   eternity_src  += files('linux/i_cpu_posix.cpp')
elif build_machine.system() == 'windows'
   subdir('Win32')
   eternity_args += '-DEE_FEATURE_REGISTRYSCAN'
   eternity_src  += win32_src

   if meson.get_compiler('cpp').get_id() == 'msvc'
      eternity_args += ['-DEE_FEATURE_MIDIRPC', '-DEE_FEATURE_XINPUT']
   endif
endif

eternity_exe = 
executable('eternity', eternity_src,
           cpp_args :     eternity_args,
           dependencies : eternity_deps,
           install :      true)

## EOF

