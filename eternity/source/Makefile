############################################################################
# Eternity Makefile

MODE = RELEASE
O = $(O_$(MODE))
O_RELEASE = djobj
O_DEBUG = djdebug

################################ DJGPP ####################################

PLATFORM = djgpp
        
# compiler
CC=gcc
        
# the exe file name -sf
EXE=$(O)/eternity.exe
        
# options common to all builds
# Note on NO_FCVT: My version of DJGPP doesn't have fcvt. If yours does, you 
# can remove -DNO_FCVT to save a couple kb of redundant code.
CFLAGS_COMMON=-Wall -DNO_FCVT

# new features; comment out what you don't want at the moment
CFLAGS_NEWFEATURES=-DPOLYOBJECTS -DOVER_UNDER -DR_PORTALS -DAMX_NODYNALOAD -DEE_CDROM_SUPPORT -DR_LINKEDPORTALS
        
# debug options
CFLAGS_DEBUG=-g -DRANGECHECK -DINSTRUMENTED
LDFLAGS_DEBUG=
        
# optimized (release) options
CFLAGS_RELEASE=-O3 -ffast-math -fomit-frame-pointer -m486 -mreg-alloc=adcbSDB
LDFLAGS_RELEASE=
# -s
       
# libraries to link in
LIBS=-lalleg -lm -lemu
        
# this selects flags based on debug and release tagets
CFLAGS=$(CFLAGS_COMMON)  $(CFLAGS_$(MODE)) $(CFLAGS_NEWFEATURES)
LDFLAGS=$(LDFLAGS_COMMON) $(LDFLAGS_$(MODE))
        
# system-specific object files
PLATOBJS =                  \
	$(O)/i_main.o       \
	$(O)/i_system.o     \
	$(O)/i_sound.o      \
	$(O)/i_video.o      \
	$(O)/i_net.o        \
	$(O)/ser_main.o     \
	$(O)/ser_port.o     \
	$(O)/keyboard.o     \
	$(O)/mmus2mid.o     \
	$(O)/pproblit.o     \
	$(O)/emu8kmid.o

# haleyjd: this is currently out of sync with the C version, needs fixing
#	     \
#	$(O)/drawspan.o     \
#	$(O)/drawcol.o


############################## OBJECT FILES #################################

# subdirectory for objects (depends on target, to allow you
# to build debug and release versions simultaneously)

# object files
OBJS =   \
	$(PLATOBJS) \
	$(O)/acs_intr.o \
	$(O)/amx.o \
	$(O)/amxcore.o \
	$(O)/am_color.o \
	$(O)/am_map.o \
	$(O)/a_fixed.o \
	$(O)/a_small.o \
	$(O)/c_cmd.o \
	$(O)/c_io.o \
	$(O)/c_net.o \
	$(O)/c_runcmd.o \
	$(O)/doomdef.o \
	$(O)/doomstat.o \
	$(O)/dstrings.o \
	$(O)/d_deh.o \
	$(O)/d_dehtbl.o \
	$(O)/d_dialog.o \
	$(O)/d_gi.o \
	$(O)/d_io.o \
	$(O)/d_items.o \
	$(O)/d_main.o \
	$(O)/d_net.o \
	$(O)/e_cmd.o \
	$(O)/e_edf.o \
	$(O)/e_exdata.o \
	$(O)/e_lib.o \
	$(O)/e_sound.o \
	$(O)/e_states.o \
	$(O)/e_string.o \
	$(O)/e_things.o \
	$(O)/e_ttypes.o \
	$(O)/f_finale.o \
	$(O)/f_wipe.o \
	$(O)/g_bind.o \
	$(O)/g_cmd.o \
	$(O)/g_dmflag.o \
	$(O)/g_game.o \
	$(O)/g_gfs.o \
	$(O)/hi_stuff.o \
	$(O)/hu_frags.o \
	$(O)/hu_over.o \
	$(O)/hu_stuff.o \
	$(O)/info.o \
	$(O)/in_lude.o \
	$(O)/mn_emenu.o \
	$(O)/mn_engin.o \
	$(O)/mn_files.o \
	$(O)/mn_htic.o \
	$(O)/mn_menus.o \
	$(O)/mn_misc.o \
	$(O)/mn_skinv.o \
	$(O)/m_argv.o \
	$(O)/m_bbox.o \
	$(O)/m_cheat.o \
	$(O)/m_fcvt.o \
	$(O)/m_misc.o \
	$(O)/m_qstr.o \
	$(O)/m_queue.o \
	$(O)/m_random.o \
	$(O)/polyobj.o \
	$(O)/psnprntf.o \
	$(O)/p_anim.o \
	$(O)/p_ceilng.o \
	$(O)/p_chase.o \
	$(O)/p_cmd.o \
	$(O)/p_doors.o \
	$(O)/p_enemy.o \
	$(O)/p_floor.o \
	$(O)/p_genlin.o \
	$(O)/p_henemy.o \
	$(O)/p_hubs.o \
	$(O)/p_info.o \
	$(O)/p_inter.o \
	$(O)/p_lights.o \
	$(O)/p_map.o \
	$(O)/p_map3d.o \
	$(O)/p_maputl.o \
	$(O)/p_mobj.o \
	$(O)/p_partcl.o \
	$(O)/p_plats.o \
	$(O)/p_portal.o \
	$(O)/p_pspr.o \
	$(O)/p_saveg.o \
	$(O)/p_setup.o \
	$(O)/p_sight.o \
	$(O)/p_skin.o \
	$(O)/p_spec.o \
	$(O)/p_switch.o \
	$(O)/p_telept.o \
	$(O)/p_tick.o \
	$(O)/p_user.o \
	$(O)/r_bsp.o \
	$(O)/r_data.o \
	$(O)/r_draw.o \
	$(O)/r_main.o \
	$(O)/r_plane.o \
	$(O)/r_portal.o \
	$(O)/r_ripple.o \
	$(O)/r_segs.o \
	$(O)/r_sky.o \
	$(O)/r_span.o \
	$(O)/r_things.o \
	$(O)/sounds.o \
	$(O)/st_hbar.o \
	$(O)/st_lib.o \
	$(O)/st_stuff.o \
	$(O)/s_sndseq.o \
	$(O)/s_sound.o \
	$(O)/tables.o \
	$(O)/v_block.o \
	$(O)/v_font.o \
	$(O)/v_misc.o \
	$(O)/v_patch.o \
	$(O)/v_video.o \
	$(O)/wi_stuff.o \
	$(O)/w_wad.o \
	$(O)/z_zone.o \
	$(O)/confuse.o \
	$(O)/lexer.o

build : $(EXE)

$(EXE): $(OBJS) $(O)/version.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/version.o -o $@ $(LIBS)

debug:
	$(MAKE) MODE=DEBUG

$(O)/%.o:   %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o:   $(PLATFORM)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
	
$(O)/%.o:   Confuse/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o:   $(PLATFORM)/%.s
	$(CC) $(CFLAGS) -c $< -o $@

# individual file depedencies follow

# dependences are currently broken -- haleyjd