//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:  
//    Lexer header for custom libConfuse lexer.
//
//-----------------------------------------------------------------------------

#ifndef LEXER_H__
#define LEXER_H__

extern char *mytext; // haleyjd

int   mylex(cfg_t *cfg);
void  lexer_init(struct DWFILE_s *);
void  lexer_reset(void);
void  lexer_set_unquoted_spaces(boolean);
char *cfg_lexer_open(const char *filename, int lumpnum, size_t *len);
char *cfg_lexer_mustopen(cfg_t *cfg, const char *filename, int lumpnum, size_t *len);
int   cfg_lexer_include(cfg_t *cfg, char *buffer, const char *fname, int lumpnum);
int   cfg_lexer_source_type(cfg_t *cfg);

#endif

// EOF

