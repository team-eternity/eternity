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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: Generic EDF metatable builder. Has features such as inheritance.
// Authors: Ioan Chera
//

#include "z_zone.h"
#include "Confuse/confuse.h"
#include "e_edfmetatable.h"
#define NEED_EDF_DEFINITIONS
#include "e_lib.h"
#include "metaapi.h"

constexpr const char ITEM_GENERIC_TITLE_SUPER[] = "super";

// clang-format off

//
// Title properties
//
cfg_opt_t edf_generic_tprops[] =
{
    CFG_STR(ITEM_GENERIC_TITLE_SUPER, "", CFGF_NONE),
    CFG_END()
};

// clang-format on

//
// The context we work with
//
struct context_t
{
    cfg_t      *cfg;       // top-level configuration
    const char *secname;   // name of a section
    const char *deltaname; // name of a delta section
    MetaTable  *table;     // pointer to top-level (global) metatable
};

//
// Populates a table with data given in it and in inheritors.
//
static void E_populateTable(const context_t &ctx, cfg_t *sec, MetaTable &table, MetaTable &visited)
{
    visited.addInt(table.getKey(), 1);
    const char *superclass = nullptr;
    if(cfg_size(sec, "#title"))
    {
        cfg_t *titleprops;
        titleprops = cfg_gettitleprops(sec);
        if(titleprops)
            superclass = cfg_getstr(titleprops, ITEM_GENERIC_TITLE_SUPER);
    }
    bool      hasancestor = false;
    MetaTable super(estrnonempty(superclass) ? superclass : "default");
    if(estrnonempty(superclass) && !visited.hasKey(superclass))
    {
        cfg_t *ancestor = cfg_gettsec(ctx.cfg, ctx.secname, superclass);
        if(ancestor)
        {
            E_populateTable(ctx, ancestor, super, visited);
            hasancestor = true;
        }
    }
    E_MetaTableFromCfg(sec, &table, hasancestor ? &super : nullptr);
    visited.removeInt(table.getKey());
}

//
// Adds an individual section
//
static void E_addSection(const context_t &ctx, cfg_t *sec)
{
    const char *name  = cfg_title(sec);
    auto        table = ctx.table->getObjectKeyAndTypeEx<MetaTable>(name);
    if(!table)
    {
        table = new MetaTable(name);
        ctx.table->addObject(table);
    }

    MetaTable visited; // keep track of visited names
    E_populateTable(ctx, sec, *table, visited);
}

//
// Adds a delta structure.
//
static void E_addDelta(const context_t &ctx, cfg_t *sec)
{
    const char *name = cfg_getstr(sec, "name");
    if(estrempty(name)) // invalid name?
        return;
    auto table = ctx.table->getObjectKeyAndTypeEx<MetaTable>(name);
    if(!table) // nothing to delta
        return;
    MetaTable base(*table); // store the base entries in a copy

    // Update table
    E_MetaTableFromCfg(sec, table, &base);
}

//
// Builds a given metatable from an EDF file, using sections named secname
//
void E_BuildGlobalMetaTableFromEDF(cfg_t *cfg, const char *secname, const char *deltaname, MetaTable &table)
{
    edefstructvar(context_t, ctx);
    ctx.cfg       = cfg;
    ctx.secname   = secname;
    ctx.deltaname = deltaname;
    ctx.table     = &table;

    unsigned numSections = cfg_size(cfg, secname);
    for(unsigned i = 0; i < numSections; ++i)
        E_addSection(ctx, cfg_getnsec(cfg, secname, i));

    // Now check the deltas
    if(estrempty(deltaname))
        return;
    unsigned numDeltas = cfg_size(cfg, deltaname);
    for(unsigned i = 0; i < numDeltas; ++i)
        E_addDelta(ctx, cfg_getnsec(cfg, deltaname, i));
}

// EOF
