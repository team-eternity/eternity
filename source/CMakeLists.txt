## Copyright (c) 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
## Copyright (C) 2020 Max Waine
##
## This software is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This software is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor,
## Boston, MA  02110-1301  USA
##
################################################################################
######################### Set Project Source  ##################################

#
# Copyright(C) 2016-2019 Samuel Villarreal
# This macro is used under terms of GPLv2+
#
macro(add_sources)
   set(_current_source_group)
   set(_expecting_file FALSE)
   set(_expecting_group_name FALSE)

   foreach(ARG ${ARGN})
      if(${ARG} STREQUAL "SOURCE_GROUP")
         set(_expecting_group_name TRUE)
      elseif(_expecting_group_name)
         set(_current_source_group ${ARG})
         string(REPLACE " " "_" _current_source_group_var ${_current_source_group})
         list(FIND SOURCE_GROUPS ${_current_source_group_var} _group_index)

         if(_group_index EQUAL -1)
            list(APPEND SOURCE_GROUPS ${_current_source_group_var})
            set(SOURCE_GROUP_${_current_source_group_var})
         endif()

         set(_expecting_group_name FALSE)
         set(_expecting_file TRUE)
      elseif(_expecting_file)
         if(NOT _current_source_group)
            message(FATAL_ERROR "No source group name defined")
         endif()

         list(APPEND SOURCE_GROUP_${_current_source_group_var} ${ARG})

         if(NOT ${_current_source_group} STREQUAL "Root")
            source_group(${_current_source_group} FILES ${ARG})
         else()
            source_group("" FILES ${ARG})
         endif()

         if(EXISTS ${ARG})
            target_sources("${_TARGET}" PRIVATE ${ARG})
         endif()
      endif()
   endforeach()
endmacro()

function(eternity_target_compile_definitions _TARGET)
   # General flags.
   target_compile_definitions("${_TARGET}" PRIVATE _SDL_VER PUBLIC _CONSOLE HAVE_SPCLIB HAVE_ADLMIDILIB)

   # Library search flags.
   if(OPENGL_LIBRARY)
      target_compile_definitions("${_TARGET}" PRIVATE EE_FEATURE_OPENGL)
   endif()

   # Build specific flags.
   if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_definitions("${_TARGET}" PRIVATE _DEBUG RANGECHECK INSTRUMENTED ZONEIDCHECK)
   endif()

   # System specific flags.
   if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_compile_definitions("${_TARGET}" PRIVATE LINUX)
   endif()

   if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      target_compile_definitions("${_TARGET}" PRIVATE EE_FEATURE_REGISTRYSCAN)

      if(MSVC)
         target_compile_definitions("${_TARGET}" PRIVATE EE_FEATURE_MIDIRPC EE_FEATURE_XINPUT)
      endif()
   endif()
endfunction()

# Source files.

function(eternity_target_sources _TARGET)
   if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      configure_file(Win32/Eternity.rc.in "${CMAKE_CURRENT_BINARY_DIR}/Eternity.rc" @ONLY)
      add_sources(
         SOURCE_GROUP "Resource Files"
         "${CMAKE_CURRENT_SOURCE_DIR}/Win32/ee.ico"
         SOURCE_GROUP "Source Files"
         "${CMAKE_CURRENT_BINARY_DIR}/Eternity.rc"
         SOURCE_GROUP "Source Files\\\\Win32\\\\Win32 Headers"
         "${CMAKE_CURRENT_SOURCE_DIR}/Win32/i_fnames.h"
         "${CMAKE_CURRENT_SOURCE_DIR}/Win32/i_winversion.h"
         "${CMAKE_CURRENT_SOURCE_DIR}/Win32/i_xinput.h"
         SOURCE_GROUP "Source Files\\\\Win32\\\\Win32 Source"
         "${CMAKE_CURRENT_SOURCE_DIR}/Win32/i_exception.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/Win32/i_fnames.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/Win32/i_w32gui.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/Win32/i_w32main.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/Win32/i_xinput.cpp"
      )
      if(MSVC)
         if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            add_custom_command(OUTPUT midiproc_c.c midiproc_s.c midiproc.h
            COMMAND midl -Oicf -W1 -Zp8 /env amd64 /protocol dce /app_config /out ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../midiproc/midiproc.idl
            MAIN_DEPENDENCY ../midiproc/midiproc.idl)
         elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            add_custom_command(OUTPUT midiproc_c.c midiproc_s.c midiproc.h
               COMMAND midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config /out ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../midiproc/midiproc.idl
               MAIN_DEPENDENCY ../midiproc/midiproc.idl)
         endif()
         target_include_directories("${_TARGET}" PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

         add_sources(
            SOURCE_GROUP "Source Files\\\\SDL\\\\midiproc"
            "${CMAKE_CURRENT_BINARY_DIR}/midiproc_c.c"
         )

         target_include_directories("${_TARGET}" PRIVATE Win32/)
         target_include_directories("${_TARGET}" PRIVATE ../midiproc/)
      endif()
   endif()

   # macOS has no unique files currently
   #if(APPLE)
   #   include_directories(sdl/macosx/)
   #   target_sources("${_TARGET}" PRIVATE
   #      sdl/macosx/
   #   )
   #endif(APPLE)

   if(LINUX)
      target_include_directories("${_TARGET}" PRIVATE linux/)
      target_sources("${_TARGET}" PRIVATE
         linux/i_cpu_posix.cpp
      )
   endif(LINUX)

   target_include_directories("${_TARGET}" PRIVATE ./)
   target_include_directories("${_TARGET}" PRIVATE Confuse/)
   target_include_directories("${_TARGET}" PRIVATE gl/)
   target_include_directories("${_TARGET}" PRIVATE hal/)
   target_include_directories("${_TARGET}" PRIVATE sdl/)
   target_include_directories("${_TARGET}" PRIVATE textscreen/)


   add_sources(
      SOURCE_GROUP "Source Files\\\\A_\\\\A_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_args.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_common.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_doom.h"
      SOURCE_GROUP "Source Files\\\\A_\\\\A_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_common.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_counters.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_decorate.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_doom.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_general.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_heretic.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_hexen.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_weaponsdoom.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/a_weaponsheretic.cpp"
      SOURCE_GROUP "Source Files\\\\ACS_"
      "${CMAKE_CURRENT_SOURCE_DIR}/acs_func.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/acs_intr.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/act_intr.h"
      SOURCE_GROUP "Source Files\\\\AM_"
      "${CMAKE_CURRENT_SOURCE_DIR}/am_color.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/am_map.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/am_map.h"
      SOURCE_GROUP "Source Files\\\\C_\\\\C_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/c_batch.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/c_io.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/c_net.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/c_runcmd.h"
      SOURCE_GROUP "Source Files\\\\C_\\\\C_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/c_batch.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/c_cmd.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/c_io.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/c_net.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/c_runcmd.cpp"
      SOURCE_GROUP "Source Files\\\\Cam"
      "${CMAKE_CURRENT_SOURCE_DIR}/cam_aim.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/cam_common.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/cam_common.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/cam_shoot.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/cam_sight.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/cam_sight.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/cam_use.cpp"
      SOURCE_GROUP "Source Files\\\\Confuse"
      "${CMAKE_CURRENT_SOURCE_DIR}/Confuse/confuse.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/Confuse/confuse.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/Confuse/lexer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/Confuse/lexer.h"
      SOURCE_GROUP "Source Files\\\\D_\\\\D_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_deh.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_dehtbl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_diskfile.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_dwfile.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_english.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_event.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_files.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_findiwads.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_french.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_gi.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_io.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_items.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_iwad.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_keywds.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_main.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_mod.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_net.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_player.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_textur.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_think.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_ticcmd.h"
      SOURCE_GROUP "Source Files\\\\D_\\\\D_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_deh.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_dehtbl.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_diskfile.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_files.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_findiwads.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_gi.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_io.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_items.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_iwad.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_main.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/d_net.cpp"
      SOURCE_GROUP "Source Files\\\\doom"
      "${CMAKE_CURRENT_SOURCE_DIR}/dhticstr.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/doomdata.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/doomdef.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/doomdef.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/doomstat.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/doomstat.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/doomtype.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/dstrings.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/dstrings.h"
      SOURCE_GROUP "Source Files\\\\E_\\\\E_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_anim.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_args.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_compatibility.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_dstate.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_edf.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_edfmetatable.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_exdata.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_fonts.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_gameprops.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_hash.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_hashkeys.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_inventory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_lib.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_metastate.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_mod.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_player.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_puff.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_reverbs.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_rtti.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_sound.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_sprite.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_states.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_string.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_switch.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_things.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_ttypes.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_udmf.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_weapons.h"
      SOURCE_GROUP "Source Files\\\\E_\\\\E_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_anim.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_args.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_cmd.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_compatibility.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_dstate.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_edf.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_edfmetatable.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_exdata.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_fonts.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_gameprops.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_hash.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_inventory.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_lib.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_mod.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_player.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_puff.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_reverbs.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_rtti.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_sound.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_sprite.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_states.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_string.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_switch.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_things.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_ttypes.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_udmf.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/e_weapons.cpp"
      SOURCE_GROUP "Source Files\\\\EV_\\\\EV_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_actions.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_bindings.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_macros.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_sectors.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_specials.h"
      SOURCE_GROUP "Source Files\\\\EV_\\\\EV_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_actions.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_bindings.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_sectors.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_specials.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ev_static.cpp"
      SOURCE_GROUP "Source Files\\\\F_"
      "${CMAKE_CURRENT_SOURCE_DIR}/f_finale.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/f_finale.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/f_wipe.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/f_wipe.h"
      SOURCE_GROUP "Source Files\\\\G_\\\\G_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_bind.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_demolog.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_dmflag.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_game.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_gfs.h"
      SOURCE_GROUP "Source Files\\\\G_\\\\G_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_bind.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_cmd.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_demolog.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_dmflag.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_game.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/g_gfs.cpp"
      SOURCE_GROUP "Source Files\\\\GL\\\\GL Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_includes.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_init.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_primitives.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_projection.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_texture.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_vars.h"
      SOURCE_GROUP "Source Files\\\\GL\\\\GL Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_init.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_primitives.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_projection.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_texture.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/gl/gl_vars.cpp"
      SOURCE_GROUP "Source Files\\\\HAL\\\\HAL Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_directory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_gamepads.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_picker.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_platform.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_timer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/i_video.h"
      SOURCE_GROUP "Source Files\\\\HAL\\\\HAL Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_directory.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_gamepads.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_platform.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_timer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hal/i_video.cpp"
      SOURCE_GROUP "Source Files\\\\HU_\\\\HU_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_boom.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_frags.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_inventory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_modern.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_over.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_stuff.h"
      SOURCE_GROUP "Source Files\\\\HU_\\\\HU_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_boom.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_frags.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_inventory.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_modern.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_over.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hu_stuff.cpp"
      SOURCE_GROUP "Source Files\\\\IN"
      "${CMAKE_CURRENT_SOURCE_DIR}/hi_stuff.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/hi_stuff.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/in_lude.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/in_lude.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/wi_stuff.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/wi_stuff.h"
      SOURCE_GROUP "Source Files\\\\M_\\\\M_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_argv.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_avltree.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_bbox.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_bdlist.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_binary.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_buffer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_cheat.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_collection.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_compare.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_ctype.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_debug.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_dllist.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_fcvt.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_fixed.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_hash.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_misc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_qstr.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_qstrkeys.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_queue.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_random.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_shots.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_strcasestr.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_structio.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_swap.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_syscfg.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_vector.h"
      SOURCE_GROUP "Source Files\\\\M_\\\\M_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_argv.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_bbox.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_buffer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_cheat.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_debug.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_fcvt.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_hash.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_misc.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_qstr.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_queue.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_random.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_shots.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_strcasestr.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_syscfg.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_utils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/m_vector.cpp"
      SOURCE_GROUP "Source Files\\\\MetaAPI"
      "${CMAKE_CURRENT_SOURCE_DIR}/metaapi.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/metaapi.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/metaqstring.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/metaqstring.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/metaspawn.h"
      SOURCE_GROUP "Source Files\\\\Misc\\\\Misc Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/autopalette.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/info.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/psnprntf.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/resource.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tables.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/version.h"
      SOURCE_GROUP "Source Files\\\\Misc\\\\Misc Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/info.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/psnprntf.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/tables.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/version.cpp"
      SOURCE_GROUP "Source Files\\\\Mn_\\\\Mn_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_emenu.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_engin.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_files.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_htic.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_items.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_menus.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_misc.h"
      SOURCE_GROUP "Source Files\\\\Mn_\\\\Mn_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_emenu.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_engin.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_files.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_htic.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_items.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_menus.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_misc.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/mn_skinv.cpp"
      SOURCE_GROUP "Source Files\\\\P_\\\\P_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/linkoffs.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_anim.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_chase.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_enemy.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_hubs.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_info.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_inter.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_map.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_map3d.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_maputl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_mobj.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_mobjcol.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_partcl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_potal.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_portalblockmap.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_portalclip.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_portalcross.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_pspr.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_pushers.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_saveg.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_scroll.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_sector.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_setup.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_skin.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_slops.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_spec.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_things.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_tick.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_user.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_xenemy.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/polyobj.h"
      SOURCE_GROUP "Source Files\\\\P_\\\\P_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_anim.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_ceilng.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_chase.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_cmd.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_doors.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_enemy.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_floor.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_genlin.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_hubs.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_info.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_inter.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_lights.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_map.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_map3d.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_maputl.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_mobj.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_mobjcol.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_partcl.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_plats.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_portal.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_portalblockmap.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_portalclip.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_portalcross.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_pspr.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_pushers.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_saveg.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_scroll.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_sector.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_setup.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_sight.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_skin.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_slopes.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_spec.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_switch.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_telept.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_things.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_tick.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_trace.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_user.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/p_xenemy.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/polyobj.cpp"
      SOURCE_GROUP "Source Files\\\\R_\\\\R_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_bsp.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_data.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_defs.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_draw.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_drawq.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_dynabsp.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_dynseg.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_interpolate.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_lighting.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_main.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_patch.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_pcheck.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_plane.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_portal.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_ripple.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_segs.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_sky.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_state.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_textur.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_things.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_voxels.h"
      SOURCE_GROUP "Source Files\\\\R_\\\\R_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_bsp.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_data.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_draw.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_drawq.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_dynabsp.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_dynseg.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_main.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_plane.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_portal.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_ripple.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_segs.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_sky.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_span.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_textur.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_things.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/r_voxels.cpp"
      SOURCE_GROUP "Source Files\\\\S_\\\\S_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_formats.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_musinfo.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_reverb.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_sndseq.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_sound.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sounds.h"
      SOURCE_GROUP "Source Files\\\\S_\\\\S_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_formats.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_musinfo.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_reverb.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_sndseq.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/s_sound.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sounds.cpp"
      SOURCE_GROUP "Source Files\\\\SDL\\\\SDL Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_main.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_midirpc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/i_net.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdlgamepads.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdlgl2d.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdltimer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdlvideo.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/i_sound.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/i_system.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/mmus2mid.h"
      SOURCE_GROUP "Source Files\\\\SDL\\\\SDL Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_input.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_main.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_midirpc.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_net.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_pcsound.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_picker.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdlgamepads.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdlgl2d.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdlmusic.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdlsound.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdltimer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sdlvideo.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_sound.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/i_system.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/mmus2mid.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdl/ser_main.cpp"
      SOURCE_GROUP "Source Files\\\\ST_"
      "${CMAKE_CURRENT_SOURCE_DIR}/st_hbar.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/st_lib.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/st_lib.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/st_stuff.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/st_stuff.h"
      SOURCE_GROUP "Source Files\\\\T_"
      "${CMAKE_CURRENT_SOURCE_DIR}/t_plane.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/t_plane.h"
      SOURCE_GROUP "Source Files\\\\TXT\\\\TXT Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/textscreen.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_button.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_checkbox.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_conditional.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_desktop.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_dropdown.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_fileselect.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_gui.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_inputbox.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_io.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_label.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_main.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_radiobutton.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_scrollpane.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_sdl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_separator.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_spinctrl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_strut.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_table.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_utf8.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_widget.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_window.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_window_action.h"
      SOURCE_GROUP "Source Files\\\\TXT\\\\TXT Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_button.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_checkbox.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_conditional.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_desktop.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_dropdown.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_fileselect.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_gui.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_inputbox.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_io.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_label.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_radiobutton.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_scrollpane.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_sdl.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_separator.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_spinctrl.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_strut.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_table.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_utf8.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_widget.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_window.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/textscreen/txt_window_action.c"
      SOURCE_GROUP "Source Files\\\\V_\\\\V_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_alloc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_block.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_buffer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_font.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_image.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_misc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_patch.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_patchfmt.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_png.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_video.h"
      SOURCE_GROUP "Source Files\\\\V_\\\\V_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_alloc.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_block.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_buffer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_font.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_image.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_misc.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_patch.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_patchfmt.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_png.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/v_video.cpp"
      SOURCE_GROUP "Source Files\\\\W_\\\\W_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_formats.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_hacks.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_iterator.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_levels.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_wad.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_zip.h"
      SOURCE_GROUP "Source Files\\\\W_\\\\W_Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_formats.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_hacks.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_levels.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_wad.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/w_zip.cpp"
      SOURCE_GROUP "Source Files\\\\XL_\\\\XL_ Headers"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_animdefs.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_emapinfo.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_mapinfo.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_musinfo.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_scripts.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_sndinfo.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_umapinfo.h"
      SOURCE_GROUP "Source Files\\\\X_\\\\XL_ Source"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_animdefs.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_emapinfo.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_mapinfo.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_musinfo.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_scripts.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_sndinfo.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/xl_umapinfo.cpp"
      SOURCE_GROUP "Source Files\\\\Z_"
      "${CMAKE_CURRENT_SOURCE_DIR}/z_auto.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/z_native.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/z_zone.h"
   )
endfunction(eternity_target_sources)

################################################################################
######################### Set Build Targets   ##################################
## Eternity is a Windows CONSOLE application, so it's entry point is main,
## not WinMain. Adding the WIN32 qualifier sets the entry point to WinMain.
add_executable(eternity)

set_target_properties(eternity
   PROPERTIES
   CXX_STANDARD          17
   CXX_STANDARD_REQUIRED ON
)

eternity_target_compile_definitions(eternity)
eternity_target_sources(eternity)

target_include_directories(eternity PRIVATE ${SDL2_INCLUDE_DIR} ${SDL2_MIXER_INCLUDE_DIR} ${SDL2_NET_INCLUDE_DIR})
target_link_libraries(eternity ${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${SDL2_NET_LIBRARY} acsvm png_static snes_spc ADLMIDI_static)

if(OPENGL_LIBRARY)
   target_link_libraries(eternity ${OPENGL_LIBRARY})
endif()

if(MSVC)
   target_link_libraries(eternity rpcrt4)

   target_compile_options(eternity PUBLIC
                          "/MP" "/utf-8" "/permissive-"
                          "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:/fp:fast>")

   target_link_options(eternity PUBLIC
                       "/DYNAMICBASE:NO" "/SAFESEH:NO"
                       "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:/MAP:$(OutDir)$(TargetName).map>")

   if(NOT MSVC_VERSION LESS  "1914")
      target_compile_options(eternity PUBLIC "/std:c++17")
   endif()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   target_link_libraries(eternity stdc++fs)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_link_libraries(eternity version)
endif()

install(TARGETS eternity
        RUNTIME DESTINATION ${BIN_DIR}
        LIBRARY DESTINATION ${LIB_DIR}
        ARCHIVE DESTINATION ${LIB_DIR})

install(DIRECTORY ${CMAKE_SOURCE_DIR}/base
        DESTINATION ${SHARE_DIR}
        PATTERN "delete.me" EXCLUDE
        PATTERN ".svn" EXCLUDE)

## EOF

