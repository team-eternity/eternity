# Microsoft Developer Studio Project File - Name="Eternity" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Eternity - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Eternity.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Eternity.mak" CFG="Eternity - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Eternity - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Eternity - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Eternity - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "C:\Software Dev\SDL-1.2.14\include" /I "source\win32" /D "NDEBUG" /D "R_LINKEDPORTALS" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_SDL_VER" /D "AMX_NODYNALOAD" /D "EE_CDROM_SUPPORT" /D "HAVE_SPCLIB" /D "ZONE_NATIVE" /D "HAVE_STDINT_H" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 snes_spc/Release/snes_spc.lib kernel32.lib sdl.lib sdl_mixer.lib sdl_net.lib oldnames.lib msvcrt.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /map /machine:I386 /nodefaultlib
# SUBTRACT LINK32 /profile

!ELSEIF  "$(CFG)" == "Eternity - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "C:\Software Dev\SDL-1.2.14\include" /I "source\win32" /D "_DEBUG" /D "RANGECHECK" /D "INSTRUMENTED" /D "ZONEIDCHECK" /D "R_PORTALS" /D "R_LINKEDPORTALS" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_SDL_VER" /D "AMX_NODYNALOAD" /D "EE_CDROM_SUPPORT" /D "HAVE_SPCLIB" /D "ZONE_NATIVE" /D "HAVE_STDINT_H" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 snes_spc/Debug/snes_spc.lib kernel32.lib sdl.lib sdl_mixer.lib sdl_net.lib oldnames.lib msvcrt.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib
# SUBTRACT LINK32 /profile

!ENDIF 

# Begin Target

# Name "Eternity - Win32 Release"
# Name "Eternity - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter ""
# Begin Group "ACS_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\acs_intr.c
# End Source File
# Begin Source File

SOURCE=.\source\acs_intr.h
# End Source File
# End Group
# Begin Group "AM_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\am_color.c
# End Source File
# Begin Source File

SOURCE=.\Source\am_map.c
# End Source File
# Begin Source File

SOURCE=.\Source\am_map.h
# End Source File
# End Group
# Begin Group "AMX"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\a_fixed.c
# End Source File
# Begin Source File

SOURCE=.\Source\a_small.c
# End Source File
# Begin Source File

SOURCE=.\Source\a_small.h
# End Source File
# Begin Source File

SOURCE=.\Source\amx.c
# End Source File
# Begin Source File

SOURCE=.\Source\amx.h
# End Source File
# Begin Source File

SOURCE=.\Source\amxcore.c
# End Source File
# Begin Source File

SOURCE=.\Source\osdefs.h
# End Source File
# End Group
# Begin Group "C_"

# PROP Default_Filter ""
# Begin Group "C_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\c_cmd.c
# End Source File
# Begin Source File

SOURCE=.\Source\c_io.c
# End Source File
# Begin Source File

SOURCE=.\Source\c_net.c
# End Source File
# Begin Source File

SOURCE=.\Source\c_runcmd.c
# End Source File
# End Group
# Begin Group "C_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\c_io.h
# End Source File
# Begin Source File

SOURCE=.\Source\c_net.h
# End Source File
# Begin Source File

SOURCE=.\Source\c_runcmd.h
# End Source File
# End Group
# End Group
# Begin Group "Confuse"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Confuse\confuse.c
# End Source File
# Begin Source File

SOURCE=.\Source\Confuse\confuse.h
# End Source File
# Begin Source File

SOURCE=.\Source\Confuse\lexer.c
# End Source File
# End Group
# Begin Group "D_"

# PROP Default_Filter ""
# Begin Group "D_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\d_deh.c

!IF  "$(CFG)" == "Eternity - Win32 Release"

# ADD CPP /O2

!ELSEIF  "$(CFG)" == "Eternity - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Source\d_dehtbl.c
# End Source File
# Begin Source File

SOURCE=.\Source\d_gi.c
# End Source File
# Begin Source File

SOURCE=.\Source\d_io.c
# End Source File
# Begin Source File

SOURCE=.\Source\d_items.c
# End Source File
# Begin Source File

SOURCE=.\source\d_iwad.c
# End Source File
# Begin Source File

SOURCE=.\Source\d_main.c
# End Source File
# Begin Source File

SOURCE=.\Source\d_net.c
# End Source File
# End Group
# Begin Group "D_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\d_deh.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_dehtbl.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_dialog.h
# End Source File
# Begin Source File

SOURCE=.\source\d_dwfile.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_englsh.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_event.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_french.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_gi.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_io.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_items.h
# End Source File
# Begin Source File

SOURCE=.\source\d_iwad.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_keywds.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_main.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_mod.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_net.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_player.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_textur.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_think.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_ticcmd.h
# End Source File
# End Group
# End Group
# Begin Group "doom"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\dhticstr.h
# End Source File
# Begin Source File

SOURCE=.\Source\doomdata.h
# End Source File
# Begin Source File

SOURCE=.\Source\doomdef.c
# End Source File
# Begin Source File

SOURCE=.\Source\doomdef.h
# End Source File
# Begin Source File

SOURCE=.\Source\doomstat.c
# End Source File
# Begin Source File

SOURCE=.\Source\doomstat.h
# End Source File
# Begin Source File

SOURCE=.\Source\doomtype.h
# End Source File
# Begin Source File

SOURCE=.\Source\dstrings.c
# End Source File
# Begin Source File

SOURCE=.\Source\dstrings.h
# End Source File
# End Group
# Begin Group "E_"

# PROP Default_Filter ""
# Begin Group "E_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\e_args.c
# End Source File
# Begin Source File

SOURCE=.\Source\e_cmd.c
# End Source File
# Begin Source File

SOURCE=.\Source\e_edf.c
# End Source File
# Begin Source File

SOURCE=.\Source\e_exdata.c
# End Source File
# Begin Source File

SOURCE=.\source\e_fonts.c
# End Source File
# Begin Source File

SOURCE=.\source\e_hash.c
# End Source File
# Begin Source File

SOURCE=.\Source\e_lib.c
# End Source File
# Begin Source File

SOURCE=.\source\e_mod.c
# End Source File
# Begin Source File

SOURCE=.\source\e_player.c
# End Source File
# Begin Source File

SOURCE=.\Source\e_sound.c
# End Source File
# Begin Source File

SOURCE=.\Source\e_states.c
# End Source File
# Begin Source File

SOURCE=.\Source\e_string.c
# End Source File
# Begin Source File

SOURCE=.\Source\e_things.c
# End Source File
# Begin Source File

SOURCE=.\Source\e_ttypes.c
# End Source File
# End Group
# Begin Group "E_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\e_args.h
# End Source File
# Begin Source File

SOURCE=.\Source\e_edf.h
# End Source File
# Begin Source File

SOURCE=.\Source\e_exdata.h
# End Source File
# Begin Source File

SOURCE=.\source\e_fonts.h
# End Source File
# Begin Source File

SOURCE=.\source\e_hash.h
# End Source File
# Begin Source File

SOURCE=.\Source\e_lib.h
# End Source File
# Begin Source File

SOURCE=.\source\e_mod.h
# End Source File
# Begin Source File

SOURCE=.\source\e_player.h
# End Source File
# Begin Source File

SOURCE=.\Source\e_sound.h
# End Source File
# Begin Source File

SOURCE=.\Source\e_states.h
# End Source File
# Begin Source File

SOURCE=.\Source\e_string.h
# End Source File
# Begin Source File

SOURCE=.\Source\e_things.h
# End Source File
# Begin Source File

SOURCE=.\Source\e_ttypes.h
# End Source File
# End Group
# End Group
# Begin Group "F_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\f_finale.c
# End Source File
# Begin Source File

SOURCE=.\Source\f_finale.h
# End Source File
# Begin Source File

SOURCE=.\Source\f_wipe.c
# End Source File
# Begin Source File

SOURCE=.\Source\f_wipe.h
# End Source File
# End Group
# Begin Group "G_"

# PROP Default_Filter ""
# Begin Group "G_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\g_bind.c
# End Source File
# Begin Source File

SOURCE=.\Source\g_cmd.c
# End Source File
# Begin Source File

SOURCE=.\Source\g_dmflag.c
# End Source File
# Begin Source File

SOURCE=.\Source\g_game.c
# End Source File
# Begin Source File

SOURCE=.\Source\g_gfs.c
# End Source File
# End Group
# Begin Group "G_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\g_bind.h
# End Source File
# Begin Source File

SOURCE=.\Source\g_dmflag.h
# End Source File
# Begin Source File

SOURCE=.\Source\g_game.h
# End Source File
# Begin Source File

SOURCE=.\Source\g_gfs.h
# End Source File
# End Group
# End Group
# Begin Group "HU_"

# PROP Default_Filter ""
# Begin Group "HU_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\hu_frags.c
# End Source File
# Begin Source File

SOURCE=.\Source\hu_over.c
# End Source File
# Begin Source File

SOURCE=.\Source\hu_stuff.c
# End Source File
# End Group
# Begin Group "HU_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Hu_frags.h
# End Source File
# Begin Source File

SOURCE=.\Source\Hu_over.h
# End Source File
# Begin Source File

SOURCE=.\Source\Hu_stuff.h
# End Source File
# End Group
# End Group
# Begin Group "IN"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\hi_stuff.c
# End Source File
# Begin Source File

SOURCE=.\Source\hi_stuff.h
# End Source File
# Begin Source File

SOURCE=.\Source\in_lude.c
# End Source File
# Begin Source File

SOURCE=.\Source\in_lude.h
# End Source File
# Begin Source File

SOURCE=.\Source\wi_stuff.c
# End Source File
# Begin Source File

SOURCE=.\Source\wi_stuff.h
# End Source File
# End Group
# Begin Group "M_"

# PROP Default_Filter ""
# Begin Group "M_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\m_argv.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_bbox.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_cheat.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_fcvt.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_misc.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_qstr.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_queue.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_random.c
# End Source File
# Begin Source File

SOURCE=.\source\m_syscfg.c
# End Source File
# Begin Source File

SOURCE=.\source\m_vector.c
# End Source File
# Begin Source File

SOURCE=.\source\metaapi.c
# End Source File
# End Group
# Begin Group "M_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\m_argv.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_bbox.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_cheat.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_dllist.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_fcvt.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_fixed.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_misc.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_qstr.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_queue.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_random.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_swap.h
# End Source File
# Begin Source File

SOURCE=.\source\m_syscfg.h
# End Source File
# Begin Source File

SOURCE=.\source\m_vector.h
# End Source File
# Begin Source File

SOURCE=.\source\metaapi.h
# End Source File
# End Group
# End Group
# Begin Group "Mn_"

# PROP Default_Filter ""
# Begin Group "Mn_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\mn_emenu.c
# End Source File
# Begin Source File

SOURCE=.\Source\mn_engin.c
# End Source File
# Begin Source File

SOURCE=.\source\mn_files.c
# End Source File
# Begin Source File

SOURCE=.\Source\mn_htic.c
# End Source File
# Begin Source File

SOURCE=.\Source\mn_menus.c
# End Source File
# Begin Source File

SOURCE=.\Source\mn_misc.c
# End Source File
# Begin Source File

SOURCE=.\Source\mn_skinv.c
# End Source File
# End Group
# Begin Group "Mn_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\mn_emenu.h
# End Source File
# Begin Source File

SOURCE=.\Source\mn_engin.h
# End Source File
# Begin Source File

SOURCE=.\source\mn_files.h
# End Source File
# Begin Source File

SOURCE=.\Source\mn_htic.h
# End Source File
# Begin Source File

SOURCE=.\Source\mn_menus.h
# End Source File
# Begin Source File

SOURCE=.\Source\mn_misc.h
# End Source File
# End Group
# End Group
# Begin Group "P_"

# PROP Default_Filter ""
# Begin Group "P_ Source"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\Source\p_anim.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_ceilng.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_chase.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_cmd.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_doors.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_enemy.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_floor.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_genlin.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_henemy.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_hubs.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_info.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_inter.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_lights.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_map.c
# End Source File
# Begin Source File

SOURCE=.\source\p_map3d.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_maputl.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_mobj.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_partcl.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_plats.c
# End Source File
# Begin Source File

SOURCE=.\source\p_portal.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_pspr.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_saveg.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_setup.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_sight.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_skin.c
# End Source File
# Begin Source File

SOURCE=.\source\p_slopes.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_spec.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_switch.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_telept.c
# End Source File
# Begin Source File

SOURCE=.\source\p_things.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_tick.c
# End Source File
# Begin Source File

SOURCE=.\source\p_trace.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_user.c
# End Source File
# Begin Source File

SOURCE=.\source\p_xenemy.c
# End Source File
# Begin Source File

SOURCE=.\source\polyobj.c
# End Source File
# End Group
# Begin Group "P_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\linkoffs.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_anim.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_chase.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_enemy.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_hubs.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_info.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_inter.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_map.h
# End Source File
# Begin Source File

SOURCE=.\source\p_map3d.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_maputl.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_mobj.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_partcl.h
# End Source File
# Begin Source File

SOURCE=.\source\p_portal.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_pspr.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_saveg.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_setup.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_skin.h
# End Source File
# Begin Source File

SOURCE=.\source\p_slopes.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_spec.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_tick.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_user.h
# End Source File
# Begin Source File

SOURCE=.\source\p_xenemy.h
# End Source File
# Begin Source File

SOURCE=.\source\polyobj.h
# End Source File
# End Group
# End Group
# Begin Group "R_"

# PROP Default_Filter ""
# Begin Group "R_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\r_bsp.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_data.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_draw.c
# End Source File
# Begin Source File

SOURCE=.\source\r_drawl.c
# End Source File
# Begin Source File

SOURCE=.\source\r_drawq.c
# End Source File
# Begin Source File

SOURCE=.\source\r_dynseg.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_main.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_plane.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_portal.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_ripple.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_segs.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_sky.c
# End Source File
# Begin Source File

SOURCE=.\source\r_span.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_things.c
# End Source File
# End Group
# Begin Group "R_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\r_bsp.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_data.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_defs.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_draw.h
# End Source File
# Begin Source File

SOURCE=.\source\r_drawl.h
# End Source File
# Begin Source File

SOURCE=.\source\r_drawq.h
# End Source File
# Begin Source File

SOURCE=.\source\r_dynseg.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_main.h
# End Source File
# Begin Source File

SOURCE=.\source\r_pcheck.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_plane.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_portal.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_ripple.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_segs.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_sky.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_state.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_things.h
# End Source File
# End Group
# End Group
# Begin Group "S_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\s_sndseq.c
# End Source File
# Begin Source File

SOURCE=.\source\s_sndseq.h
# End Source File
# Begin Source File

SOURCE=.\Source\s_sound.c
# End Source File
# Begin Source File

SOURCE=.\Source\s_sound.h
# End Source File
# Begin Source File

SOURCE=.\Source\sounds.c
# End Source File
# Begin Source File

SOURCE=.\Source\sounds.h
# End Source File
# End Group
# Begin Group "SDL"

# PROP Default_Filter ""
# Begin Group "SDL Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\Win32\i_cpu.c
# End Source File
# Begin Source File

SOURCE=.\source\Win32\i_exception.c
# End Source File
# Begin Source File

SOURCE=.\Source\Win32\i_fnames.c
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_input.c
# End Source File
# Begin Source File

SOURCE=.\source\Win32\i_iwad.c
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_main.c
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_net.c
# End Source File
# Begin Source File

SOURCE=.\source\Win32\i_opndir.c
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_pcsound.c
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_picker.c
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_sdlmusic.c
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_sdlsound.c
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_sound.c
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_system.c
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_video.c
# End Source File
# Begin Source File

SOURCE=.\source\Win32\i_w32main.c
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\mmus2mid.c
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\ser_main.c
# End Source File
# End Group
# Begin Group "SDL Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Win32\i_fnames.h
# End Source File
# Begin Source File

SOURCE=.\Source\i_net.h
# End Source File
# Begin Source File

SOURCE=.\source\Win32\i_opndir.h
# End Source File
# Begin Source File

SOURCE=.\Source\i_sound.h
# End Source File
# Begin Source File

SOURCE=.\Source\i_system.h
# End Source File
# Begin Source File

SOURCE=.\Source\i_video.h
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\mmus2mid.h
# End Source File
# End Group
# End Group
# Begin Group "ST_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\st_hbar.c
# End Source File
# Begin Source File

SOURCE=.\Source\st_lib.c
# End Source File
# Begin Source File

SOURCE=.\Source\st_lib.h
# End Source File
# Begin Source File

SOURCE=.\Source\st_stuff.c
# End Source File
# Begin Source File

SOURCE=.\Source\st_stuff.h
# End Source File
# End Group
# Begin Group "V_"

# PROP Default_Filter ""
# Begin Group "V_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\v_block.c
# End Source File
# Begin Source File

SOURCE=.\source\v_buffer.c
# End Source File
# Begin Source File

SOURCE=.\Source\v_font.c
# End Source File
# Begin Source File

SOURCE=.\Source\v_misc.c
# End Source File
# Begin Source File

SOURCE=.\Source\v_patch.c
# End Source File
# Begin Source File

SOURCE=.\Source\v_video.c
# End Source File
# End Group
# Begin Group "V_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\v_block.h
# End Source File
# Begin Source File

SOURCE=.\source\v_buffer.h
# End Source File
# Begin Source File

SOURCE=.\Source\v_font.h
# End Source File
# Begin Source File

SOURCE=.\Source\v_misc.h
# End Source File
# Begin Source File

SOURCE=.\Source\v_patch.h
# End Source File
# Begin Source File

SOURCE=.\Source\v_video.h
# End Source File
# End Group
# End Group
# Begin Group "W_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\w_wad.c
# End Source File
# Begin Source File

SOURCE=.\Source\w_wad.h
# End Source File
# End Group
# Begin Group "Z_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\z_native.c
# End Source File
# Begin Source File

SOURCE=.\Source\z_zone.c
# End Source File
# Begin Source File

SOURCE=.\Source\z_zone.h
# End Source File
# End Group
# Begin Group "Misc"

# PROP Default_Filter ""
# Begin Group "Misc Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\info.c
# End Source File
# Begin Source File

SOURCE=.\Source\psnprntf.c
# End Source File
# Begin Source File

SOURCE=.\Source\tables.c
# End Source File
# Begin Source File

SOURCE=.\Source\version.c
# End Source File
# End Group
# Begin Group "Misc Headers"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\Source\info.h
# End Source File
# Begin Source File

SOURCE=.\source\Win32\inttypes.h
# End Source File
# Begin Source File

SOURCE=.\Source\psnprntf.h
# End Source File
# Begin Source File

SOURCE=.\source\Win32\stdint.h
# End Source File
# Begin Source File

SOURCE=.\Source\tables.h
# End Source File
# Begin Source File

SOURCE=.\Source\version.h
# End Source File
# End Group
# End Group
# Begin Group "TXT_"

# PROP Default_Filter ""
# Begin Group "TXT Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\textscreen\txt_button.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_checkbox.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_desktop.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_dropdown.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_gui.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_inputbox.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_io.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_label.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_radiobutton.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_scrollpane.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_sdl.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_separator.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_spinctrl.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_strut.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_table.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_widget.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_window.c
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_window_action.c
# End Source File
# End Group
# Begin Group "TXT Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\textscreen\textscreen.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_button.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_checkbox.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_desktop.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_dropdown.h
# End Source File
# Begin Source File

SOURCE=.\Source\textscreen\txt_font.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_gui.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_inputbox.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_io.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_label.h
# End Source File
# Begin Source File

SOURCE=.\Source\textscreen\txt_main.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_radiobutton.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_scrollpane.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_sdl.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_separator.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_spinctrl.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_strut.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_table.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_widget.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_window.h
# End Source File
# Begin Source File

SOURCE=.\source\textscreen\txt_window_action.h
# End Source File
# End Group
# End Group
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
