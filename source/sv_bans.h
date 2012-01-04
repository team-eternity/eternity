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
//   Serverside banlist and whitelist.
//
//----------------------------------------------------------------------------

#ifndef SV_BANS_H__
#define SV_BANS_H__

#define DEFAULT_ACCESS_LIST_FILENAME "access_list.json"

enum
{
   BAN_ALREADY_EXISTS,
   BAN_NOT_FOUND,
   WHITELIST_ENTRY_ALREADY_EXISTS,
   WHITELIST_ENTRY_NOT_FOUND,
   MAX_ACCESS_LIST_ERRORS
};

extern const char *access_list_error_strings[MAX_ACCESS_LIST_ERRORS];

class AccessList
{
private:
   const char *error;
   Json::Value json;

   void setError(int error_code);
   void writeOutAccessList(void);

public:
   AccessList();
   bool         addBanListEntry(const char *address, const char *name,
                                const char *reason);
   bool         removeBanListEntry(const char *address);
   bool         addWhiteListEntry(const char *address, const char *name);
   bool         removeWhiteListEntry(const char *address);
   bool         isBanned(const char *address);
   Json::Value& getBan(const char *address);
   const char*  getError(void);
   void         printBansToConsole(void);
   void         printWhiteListsToConsole(void);

};

extern AccessList *sv_access_list;

#endif

