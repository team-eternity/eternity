//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:  
//    Lexer header for custom libConfuse lexer.
//
//-----------------------------------------------------------------------------

#ifndef LEXER_H__
#define LEXER_H__

extern const char *mytext; // haleyjd
class DWFILE;

int   mylex(cfg_t *cfg);
int   lexer_init(cfg_t *cfg, DWFILE *);
void  lexer_reset(void);
void  lexer_set_unquoted_spaces(bool);
char *cfg_lexer_open(const char *filename, int lumpnum, size_t *len);
char *cfg_lexer_mustopen(cfg_t *cfg, const char *filename, int lumpnum, size_t *len);
int   cfg_lexer_include(cfg_t *cfg, char *buffer, const char *fname, int lumpnum);
int   cfg_lexer_source_type(cfg_t *cfg);
void  cfg_lexer_set_dialect(cfg_dialect_t dialect);

#endif

// EOF

