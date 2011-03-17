
AM_CXXFLAGS = -I../ -I../textscreen @SDL_CFLAGS@ -I/usr/local/include/SDL -I/usr/include/SDL \
-D_SDL_VER -DNDEBUG -DAMX_NODYNALOAD -DR_LINKEDPORTALS -DZONE_NATIVE \
-DHAVE_SCHED_SETAFFINITY -D_CONSOLE -DLINUX

noinst_LIBRARIES = libsdlet.a

libsdlet_a_SOURCES = \
i_input.cpp     i_main.cpp   i_net.cpp    i_video.cpp    i_sound.cpp   i_system.cpp \
mmus2mid.cpp    mmus2mid.h   ser_main.cpp i_sdlsound.cpp i_pcsound.cpp i_sdlmusic.cpp \
i_cpu_posix.cpp i_picker.cpp
