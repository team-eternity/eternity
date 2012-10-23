// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 Kate Stone
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
//
//   Various miscellaneous functions for use with Python.
//
//-----------------------------------------------------------------------------

#include "ae_pyutils.h"

QStringCollection* Split (qstring& name, const char splitchar)
{
   qstring* path = new qstring (name);
   QStringCollection* pathList = new QStringCollection();

   size_t pos = name.findFirstOf (splitchar);

   while (pos != qstring::npos)
   {
      qstring* left = new qstring ();
      left->copy (*path);
      left->truncate (pos);
      pathList->add (*left);

      pos = name.findFirstOf (splitchar, pos + 1);
   }
   pathList->add (*path);

   return pathList;
}

// This is used by TextPrinter for buffering. Returns a two-item list
// (left, right), or NULL if middle is not present in str.

QStringCollection* Partition (qstring& str, char middle)
{
   QStringCollection *pair = NULL;
   qstring *left, *right;

   size_t pos = str.findFirstOf (middle);

   if (pos != qstring::npos)
   {
      pos++; // Stick the middle on the left side of the string.

      pair = new QStringCollection (2);

      if (pos < (str.length () - 1))
      {
         left = new qstring ();
         left->copy (str);
         left->truncate (pos);
         pair->add (*left);

         right = new qstring (str.getBuffer () + pos);
         pair->add (*right);
      }
      else
      {
         pair->add (str);

         qstring *dummy = new qstring ();
         pair->add (*dummy);
      }
   }

   return pair;
}
