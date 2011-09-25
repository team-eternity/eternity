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
# ADD CPP /nologo /MD /W3 /GR /GX /O2 /I "..\SDL-1.2.14\include" /I "source\win32" /D "NDEBUG" /D "R_LINKEDPORTALS" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_SDL_VER" /D "AMX_NODYNALOAD" /D "EE_CDROM_SUPPORT" /D "HAVE_SPCLIB" /D "ZONE_NATIVE" /D "HAVE_STDINT_H" /YX /FD /c
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
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\SDL-1.2.14\include" /I "source\win32" /D "_DEBUG" /D "RANGECHECK" /D "INSTRUMENTED" /D "ZONEIDCHECK" /D "R_PORTALS" /D "R_LINKEDPORTALS" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_SDL_VER" /D "AMX_NODYNALOAD" /D "EE_CDROM_SUPPORT" /D "HAVE_SPCLIB" /D "ZONE_NATIVE" /D "HAVE_STDINT_H" /YX /FD /GZ /c
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

SOURCE=.\source\acs_intr.cpp
# End Source File
# Begin Source File

SOURCE=.\source\acs_intr.h
# End Source File
# End Group
# Begin Group "AM_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\am_color.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\am_map.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\am_map.h
# End Source File
# End Group
# Begin Group "AMX"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\a_fixed.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\a_small.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\a_small.h
# End Source File
# Begin Source File

SOURCE=.\Source\amx.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\amx.h
# End Source File
# Begin Source File

SOURCE=.\Source\amxcore.cpp
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

SOURCE=.\Source\c_cmd.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\c_io.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\c_net.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\c_runcmd.cpp
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

SOURCE=.\Source\Confuse\confuse.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\Confuse\confuse.h
# End Source File
# Begin Source File

SOURCE=.\Source\Confuse\lexer.cpp
# End Source File
# End Group
# Begin Group "D_"

# PROP Default_Filter ""
# Begin Group "D_ Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\d_deh.cpp

!IF  "$(CFG)" == "Eternity - Win32 Release"

# ADD CPP /O2

!ELSEIF  "$(CFG)" == "Eternity - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Source\d_dehtbl.cpp
# End Source File
# Begin Source File

SOURCE=.\source\d_diskfile.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\d_gi.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\d_io.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\d_items.cpp
# End Source File
# Begin Source File

SOURCE=.\source\d_iwad.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\d_main.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\d_net.cpp
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

SOURCE=.\source\d_diskfile.h
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

SOURCE=.\Source\doomdef.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\doomdef.h
# End Source File
# Begin Source File

SOURCE=.\Source\doomstat.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\doomstat.h
# End Source File
# Begin Source File

SOURCE=.\Source\doomtype.h
# End Source File
# Begin Source File

SOURCE=.\Source\dstrings.cpp
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

SOURCE=.\source\e_args.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\e_cmd.cpp
# End Source File
# Begin Source File

SOURCE=.\source\e_dstate.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\e_edf.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\e_exdata.cpp
# End Source File
# Begin Source File

SOURCE=.\source\e_fonts.cpp
# End Source File
# Begin Source File

SOURCE=.\source\e_hash.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\e_lib.cpp
# End Source File
# Begin Source File

SOURCE=.\source\e_mod.cpp
# End Source File
# Begin Source File

SOURCE=.\source\e_player.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\e_sound.cpp
# End Source File
# Begin Source File

SOURCE=.\source\e_sprite.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\e_states.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\e_string.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\e_things.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\e_ttypes.cpp
# End Source File
# End Group
# Begin Group "E_ Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\e_args.h
# End Source File
# Begin Source File

SOURCE=.\source\e_dstate.h
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

SOURCE=.\source\e_sprite.h
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

SOURCE=.\Source\f_finale.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\f_finale.h
# End Source File
# Begin Source File

SOURCE=.\Source\f_wipe.cpp
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

SOURCE=.\Source\g_bind.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\g_cmd.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\g_dmflag.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\g_game.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\g_gfs.cpp
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

SOURCE=.\Source\hu_frags.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\hu_over.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\hu_stuff.cpp
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

SOURCE=.\Source\hi_stuff.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\hi_stuff.h
# End Source File
# Begin Source File

SOURCE=.\Source\in_lude.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\in_lude.h
# End Source File
# Begin Source File

SOURCE=.\Source\wi_stuff.cpp
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

SOURCE=.\Source\m_argv.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\m_bbox.cpp
# End Source File
# Begin Source File

SOURCE=.\source\m_buffer.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\m_cheat.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\m_fcvt.cpp
# End Source File
# Begin Source File

SOURCE=.\source\m_hash.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\m_misc.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\m_qstr.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\m_queue.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\m_random.cpp
# End Source File
# Begin Source File

SOURCE=.\source\m_shots.cpp
# End Source File
# Begin Source File

SOURCE=.\source\m_syscfg.cpp
# End Source File
# Begin Source File

SOURCE=.\source\m_vector.cpp
# End Source File
# Begin Source File

SOURCE=.\source\metaapi.cpp
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

SOURCE=.\source\m_buffer.h
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

SOURCE=.\source\m_hash.h
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

SOURCE=.\source\m_shots.h
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

SOURCE=.\source\mn_emenu.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\mn_engin.cpp
# End Source File
# Begin Source File

SOURCE=.\source\mn_files.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\mn_htic.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\mn_menus.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\mn_misc.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\mn_skinv.cpp
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

SOURCE=.\Source\p_anim.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_ceilng.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_chase.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_cmd.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_doors.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_enemy.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_floor.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_genlin.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_henemy.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_hubs.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_info.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_inter.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_lights.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_map.cpp
# End Source File
# Begin Source File

SOURCE=.\source\p_map3d.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_maputl.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_mobj.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_partcl.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_plats.cpp
# End Source File
# Begin Source File

SOURCE=.\source\p_portal.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_pspr.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_saveg.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_setup.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_sight.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_skin.cpp
# End Source File
# Begin Source File

SOURCE=.\source\p_slopes.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_spec.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_switch.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_telept.cpp
# End Source File
# Begin Source File

SOURCE=.\source\p_things.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_tick.cpp
# End Source File
# Begin Source File

SOURCE=.\source\p_trace.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\p_user.cpp
# End Source File
# Begin Source File

SOURCE=.\source\p_xenemy.cpp
# End Source File
# Begin Source File

SOURCE=.\source\polyobj.cpp
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

SOURCE=.\Source\r_bsp.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\r_data.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\r_draw.cpp
# End Source File
# Begin Source File

SOURCE=.\source\r_drawl.cpp
# End Source File
# Begin Source File

SOURCE=.\source\r_drawq.cpp
# End Source File
# Begin Source File

SOURCE=.\source\r_dynseg.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\r_main.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\r_plane.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\r_portal.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\r_ripple.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\r_segs.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\r_sky.cpp
# End Source File
# Begin Source File

SOURCE=.\source\r_span.cpp
# End Source File
# Begin Source File

SOURCE=.\source\r_textur.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\r_things.cpp
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

SOURCE=.\source\s_sndseq.cpp
# End Source File
# Begin Source File

SOURCE=.\source\s_sndseq.h
# End Source File
# Begin Source File

SOURCE=.\Source\s_sound.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\s_sound.h
# End Source File
# Begin Source File

SOURCE=.\Source\sounds.cpp
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

SOURCE=.\source\Win32\i_cpu.cpp
# End Source File
# Begin Source File

SOURCE=.\source\Win32\i_exception.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\Win32\i_fnames.cpp
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_input.cpp
# End Source File
# Begin Source File

SOURCE=.\source\Win32\i_iwad.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_main.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_net.cpp
# End Source File
# Begin Source File

SOURCE=.\source\Win32\i_opndir.cpp
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_pcsound.cpp
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_picker.cpp
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_sdlmusic.cpp
# End Source File
# Begin Source File

SOURCE=.\source\sdl\i_sdlsound.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_sound.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_system.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\i_video.cpp
# End Source File
# Begin Source File

SOURCE=.\source\Win32\i_w32main.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\mmus2mid.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\sdl\ser_main.cpp
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

SOURCE=.\Source\st_hbar.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\st_lib.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\st_lib.h
# End Source File
# Begin Source File

SOURCE=.\Source\st_stuff.cpp
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

SOURCE=.\Source\v_block.cpp
# End Source File
# Begin Source File

SOURCE=.\source\v_buffer.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\v_font.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\v_misc.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\v_patch.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\v_video.cpp
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

SOURCE=.\source\w_levels.cpp
# End Source File
# Begin Source File

SOURCE=.\source\w_levels.h
# End Source File
# Begin Source File

SOURCE=.\Source\w_wad.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\w_wad.h
# End Source File
# End Group
# Begin Group "Z_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\z_native.cpp
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

SOURCE=.\Source\info.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\psnprntf.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\tables.cpp
# End Source File
# Begin Source File

SOURCE=.\Source\version.cpp
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
