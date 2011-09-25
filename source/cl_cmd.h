// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//   C/S client commands.
//
//----------------------------------------------------------------------------


extern unsigned int cl_packet_buffer_size;
extern unsigned int default_cl_packet_buffer_size;
extern unsigned int cl_flush_packet_buffer_on_death_speed;
extern unsigned int default_cl_flush_packet_buffer_on_death_speed;
extern unsigned int damage_screen_cap;
extern unsigned int default_damage_screen_cap;
extern bool cl_flush_packet_buffer;
extern bool cl_enable_prediction;
extern bool default_cl_enable_prediction;
extern bool cl_predict_shots;
extern bool default_cl_predict_shots;
extern bool cl_buffer_packets_while_spectating;
extern bool default_cl_buffer_packets_while_spectating;
extern bool cl_flush_packet_buffer_on_respawn;
extern bool default_cl_flush_packet_buffer_on_respawn;
extern bool cl_constant_prediction;
extern bool default_cl_constant_prediction;

void CL_AddCommands(void);

