/* Configuration file parser -*- tab-width: 4; -*-
 *
 * Copyright (c) 2002-2003, Martin Hedenfalk <mhe@home.se>
 * Modifications for Eternity (c) 2003 James Haley
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 */

/** A configuration file parser library.
 * @file confuse.h
 *
 */

/**
 * \mainpage libConfuse Documentation
 *
 * \section intro
 *
 * Copyright &copy; 2002-2003 Martin Hedenfalk &lt;mhe@home.se&gt;
 *
 * The latest versions of this manual and the libConfuse software are
 * available at http://www.e.kth.se/~e97_mhe/confuse.shtml
 *
 *
 * <em>If you can't convince, confuse.</em>
 */

#ifndef CONFUSE_H__
#define CONFUSE_H__

#include "../z_zone.h"

/** Fundamental option types */
enum cfg_type_t
{
    CFGT_NONE,
    CFGT_INT,     /**< integer */
    CFGT_FLOAT,   /**< floating point number */
    CFGT_STR,     /**< string */
    CFGT_BOOL,    /**< boolean value */
    CFGT_SEC,     /**< section */
    CFGT_FUNC,    /**< function */
    CFGT_STRFUNC, /**< function-valued string */
    CFGT_MVPROP,  /**< multi-valued property */
    CFGT_FLAG     /**< flag property, no value is given */
};

using cfg_type_t = enum cfg_type_t;
using cfg_flag_t = int;

// haleyjd 07/11/03: changed to match libConfuse 2.0 cvs
/** Flags. */
enum
{
    CFGF_NONE      = 0x00000000,
    CFGF_MULTI     = 0x00000001, /**< option may be specified multiple times */
    CFGF_LIST      = 0x00000002, /**< option is a list */
    CFGF_NOCASE    = 0x00000004, /**< configuration file is case insensitive */
    CFGF_TITLE     = 0x00000008, /**< option has a title (only applies to section) */
    CFGF_ALLOCATED = 0x00000010,
    CFGF_RESET     = 0x00000020,
    CFGF_DEFINIT   = 0x00000040,
    // haleyjd: custom flags
    CFGF_LOOKFORFUNC = 0x00000080, /**< will do nothing until "lookfor" function is found */
    CFGF_STRSPACE    = 0x00000100, /**< unquoted strings within this section may contain spaces */
    CFGF_SIGNPREFIX  = 0x00000200, /**< CFGT_FLAG items expect a + or - prefix on the property */
    CFGF_TITLEPROPS  = 0x00000400, /**< MVPROP with this flag defines the section's title properties */
};

/** Return codes from cfg_parse(). */
enum
{
    CFG_SUCCESS     = 0,
    CFG_FILE_ERROR  = -1,
    CFG_PARSE_ERROR = 1,
};

/** Dialects - haleyjd */
enum cfg_dialect_t
{
    CFG_DIALECT_DELTA,   /**< original dialect */
    CFG_DIALECT_ALFHEIM, /**< treats ':' characters as assignments */
    CFG_NUMDIALECTS      /**< keep this last */
};

inline static bool is_set(const int f, const cfg_flag_t x)
{
    return (f & x) == f;
}

union cfg_value_t;
struct cfg_opt_t;
struct cfg_t;

/** Function prototype used by CFGT_FUNC options.
 *
 * This is a callback function, registered with the CFG_FUNC
 * initializer. Each time libConfuse finds a function, the registered
 * callback function is called (parameters are passed as strings, any
 * conversion to other types should be made in the callback
 * function). libConfuse does not support any storage of the data
 * found; these are passed as parameters to the callback, and it's the
 * responsibility of the callback function to do whatever it should do
 * with the data.
 *
 * @param cfg The configuration file context.
 * @param opt The option.
 * @param argc Number of arguments passed. The callback function is
 * responsible for checking that the correct number of arguments are
 * passed.
 * @param argv Arguments as an array of character strings.
 *
 * @return On success, 0 should be returned. All other values
 * indicates an error, and the parsing is aborted. The callback
 * function should notify the error itself, for example by calling
 * cfg_error().
 *
 * @see CFG_FUNC
 */
using cfg_func_t = int (*)(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv);

/** Value parsing callback prototype
 *
 * This is a callback function (different from the one registered with
 * the CFG_FUNC initializer) used to parse a value. This can be used
 * to override the internal parsing of a value.
 *
 * Suppose you want an integer option that only can have certain
 * values, for example 1, 2 and 3, and these should be written in the
 * configuration file as "yes", "no" and "maybe". The callback
 * function would be called with the found value ("yes", "no" or
 * "maybe") as a string, and the result should be stored in the result
 * parameter.
 *
 * @param cfg The configuration file context.
 * @param opt The option.
 * @param value The value found in the configuration file.
 * @param result Pointer to storage for the result, cast to a void pointer.
 *
 * @return On success, 0 should be returned. All other values
 * indicates an error, and the parsing is aborted. The callback
 * function should notify the error itself, for example by calling
 * cfg_error().
 */
using cfg_callback_t = int (*)(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result);

/** Error reporting function. */
using cfg_errfunc_t = void (*)(const cfg_t *const cfg, const char *fmt, va_list ap);

/** Lexer open callback */
using cfg_lexfunc_t = int (*)(cfg_t *cfg, const char *data, int size);

/**
 * Data structure holding information about a "section". Sections can
 * be nested. A section has a list of options (strings, numbers,
 * booleans or other sections) grouped together.
 */
struct cfg_t
{
    cfg_flag_t  flags;      /**< Any flags passed to cfg_init() */
    const char *name;       /**< The name of this section, the root
                             * section returned from cfg_init() is
                             * always named "root" */
    char       *namealloc;  /**< Pointer to name if allocated on heap */
    cfg_opt_t  *opts;       /**< Array of options */
    const char *title;      /**< Optional title for this section, only
                             * set if CFGF_TITLE flag is set */
    char         *filename; /**< Name of the file being parsed */
    int           line;     /**< Line number in the config file */
    int           lumpnum;  /**< haleyjd: if a lump, this is its number */
    cfg_errfunc_t errfunc;  /**< This function (set with
                             * cfg_set_error_function) is called for
                             * any error message. */
    cfg_lexfunc_t lexfunc;  /**< haleyjd: A callback dispatched by the lexer
                             * when initially opening a file. */
    const char *lookfor;    /**< Name of a function to look for. */
    cfg_t      *displaced;  /**< haleyjd: pointer to a displaced section */
};

/**
 * Data structure holding the value of a fundamental option value.
 */
union cfg_value_t
{
    int    number;   /**< integer value */
    double fpnumber; /**< floating point value */
    bool   boolean;  /**< boolean value */
    char  *string;   /**< string value */
    cfg_t *section;  /**< section value */
};

/**
 * Data structure holding information about an option. The value(s)
 * are stored as an array of fundamental values (strings, numbers).
 */
struct cfg_opt_t
{
    const char   *name;      /**< The name of the option */
    cfg_type_t    type;      /**< Type of option */
    unsigned int  nvalues;   /**< Number of values parsed */
    cfg_value_t **values;    /**< Array of found values */
    cfg_flag_t    flags;     /**< Flags */
    cfg_opt_t    *subopts;   /**< Suboptions (only applies to sections) */
    int           idef;      /**< Default integer value */
    const char   *sdef;      /**< Default string value */
    bool          bdef;      /**< Default boolean value */
    double        fpdef;     /**< Default value for CFGT_FLOAT options
                              * (blargh!, I want a union here, but
                              * unions can't be statically initialized) */
    cfg_func_t func;         /**< Function callback for CFGT_FUNC options */
    void      *simple_value; /**< Pointer to user-specified variable to
                              * store simple values (created with the
                              * CFG_SIMPLE_* initializers) */
    cfg_callback_t cb;       /**< Value parsing callback function */
};

/**
 * Initialize a string option
 */
constexpr cfg_opt_t CFG_STR(const char *const name, const char *const def, const cfg_flag_t flags)
{
    return { name, CFGT_STR, 0, nullptr, flags, nullptr, 0, def, false, 0, nullptr, nullptr, nullptr };
}

constexpr cfg_opt_t CFG_STR_CB(const char *const name, const char *const def, const cfg_flag_t flags,
                               const cfg_callback_t cb)
{
    return { name, CFGT_STR, 0, nullptr, flags, nullptr, 0, def, false, 0, nullptr, nullptr, cb };
}

/** Initialize a "simple" string option.
 *
 * "Simple" options (in lack of a better expression) does not support
 * lists of values. LibConfuse will store the value of a simple option
 * in the user-defined location specified by the value parameter in
 * the initializer. Simple options are not stored in the cfg_t context
 * (you can thus not use the cfg_get* functions to get the
 * value). Sections can not be initialized as a "simple" option.
 *
 * @param name name of the option
 * @param value pointer to a character pointer (a char **). This value
 * must be initalized either to nullptr or to a malloc()'ed string. You
 * can't use CFG_SIMPLE_STR("user", "joe"), since libConfuse will try to
 * free the static string "joe" (which is an error) if a "user" option
 * is found. Rather, use the following code snippet:
 *
 * <pre>
 * char *user = strdup("joe");
 * ...
 * cfg_opt_t opts[] = {
 *      CFG_SIMPLE_STR("user", &user),
 * ...
 * </pre>
 *
 */
constexpr cfg_opt_t CFG_SIMPLE_STR(const char *const name, void *const value)
{
    return { name, CFGT_STR, 0, nullptr, CFGF_NONE, nullptr, 0, nullptr, false, 0, nullptr, value, nullptr };
}

/** Initialize an integer option
 */
constexpr cfg_opt_t CFG_INT(const char *const name, const int def, const cfg_flag_t flags)
{
    return { name, CFGT_INT, 0, nullptr, flags, nullptr, def, nullptr, false, 0, nullptr, nullptr, nullptr };
}

constexpr cfg_opt_t CFG_INT_CB(const char *const name, const int def, const cfg_flag_t flags, const cfg_callback_t cb)
{
    return { name, CFGT_INT, 0, nullptr, flags, nullptr, def, nullptr, false, 0, nullptr, nullptr, cb };
}

/** Initialize a "simple" integer option.
 */
constexpr cfg_opt_t CFG_SIMPLE_INT(const char *const name, void *const value)
{
    return { name, CFGT_INT, 0, nullptr, CFGF_NONE, nullptr, 0, nullptr, false, 0, nullptr, value, nullptr };
}

/** Initialize a floating point option
 */
constexpr cfg_opt_t CFG_FLOAT(const char *const name, const double def, const cfg_flag_t flags)
{
    return { name, CFGT_FLOAT, 0, nullptr, flags, nullptr, 0, nullptr, false, def, nullptr, nullptr, nullptr };
}

constexpr cfg_opt_t CFG_FLOAT_CB(const char *const name, const double def, const cfg_flag_t flags,
                                 const cfg_callback_t cb)
{
    return { name, CFGT_FLOAT, 0, nullptr, flags, nullptr, 0, nullptr, false, (double)def, nullptr, nullptr, cb };
}

/** Initialize a "simple" floating point option (see documentation for
 * CFG_SIMPLE_STR for more information).
 */
constexpr cfg_opt_t CFG_SIMPLE_FLOAT(const char *const name, void *const value)
{
    return { name, CFGT_FLOAT, 0, nullptr, CFGF_NONE, nullptr, 0, nullptr, false, 0, nullptr, value, nullptr };
}

/** Initialize a boolean option
 */
constexpr cfg_opt_t CFG_BOOL(const char *const name, const bool def, const cfg_flag_t flags)
{
    return { name, CFGT_BOOL, 0, nullptr, flags, nullptr, 0, nullptr, def, 0, nullptr, nullptr, nullptr };
}

constexpr cfg_opt_t CFG_BOOL_CB(const char *const name, const bool def, const cfg_flag_t flags, const cfg_callback_t cb)
{
    return { name, CFGT_BOOL, 0, nullptr, flags, nullptr, 0, nullptr, def, 0, nullptr, nullptr, cb };
}

/** Initialize a "simple" boolean option.
 */
constexpr cfg_opt_t CFG_SIMPLE_BOOL(const char *const name, void *const value)
{
    return { name, CFGT_BOOL, 0, nullptr, CFGF_NONE, nullptr, 0, nullptr, false, 0, nullptr, value, nullptr };
}

/** Initialize a section
 *
 * @param name The name of the option
 * @param opts Array of options that are valid within this section
 * @param flags Flags, specify CFGF_MULTI if it should be possible to
 * have multiples of the same section, and CFGF_TITLE if the
 * section(s) must have a title (which can be used in the
 * cfg_gettsec() function)
 */
constexpr cfg_opt_t CFG_SEC(const char *const name, cfg_opt_t *const opts, const cfg_flag_t flags)
{
    return { name, CFGT_SEC, 0, nullptr, flags, opts, 0, nullptr, false, 0, nullptr, nullptr, nullptr };
}

/** Initialize a function
 * @param name The name of the option
 * @param func The callback function.
 *
 * @see cfg_func_t
 */
constexpr cfg_opt_t CFG_FUNC(const char *const name, const cfg_func_t func)
{
    return { name, CFGT_FUNC, 0, nullptr, CFGF_NONE, nullptr, 0, nullptr, false, 0, func, nullptr, nullptr };
}

/** Initialize a function-valued string option.
 * @param name The name of the option.
 * @param def The default value.
 * @param func The callback function.
 */
constexpr cfg_opt_t CFG_STRFUNC(const char *const name, const char *const def, const cfg_func_t func)
{
    return { name, CFGT_STRFUNC, 0, nullptr, CFGF_NONE, nullptr, 0, def, false, 0, func, nullptr, nullptr };
}

/**
 * Initialize a multi-valued property option.
 *
 * @param name The name of the option.
 * @param opts Array of options in order they must appear listed.
 * @param flags Similar to CFG_SEC. Titles are not supported however.
 */
constexpr cfg_opt_t CFG_MVPROP(const char *const name, cfg_opt_t *const opts, const cfg_flag_t flags)
{
    return { name, CFGT_MVPROP, 0, nullptr, flags, opts, 0, nullptr, false, 0, nullptr, nullptr, nullptr };
}

/**
 * Initialize an MVPROP which serves to define the parent section's title
 * properties, which are an arbitrary number of items which must be defined,
 * separated by commas, after a ':' which follows the section's title.
 * Title properties must be retrieved using cfg_gettitleprops; they cannot
 * be returned by cfg_getmvprop etc. This option type only has meaning inside
 * a CFG_SEC which includes CFGF_TITLE in its flags.
 * @param opts Array of options in order they must appear listed.
 * @param flags Similar to CFG_MVPROP. CFGF_TITLEPROPS is implied.
 */
constexpr cfg_opt_t CFG_TPROPS(cfg_opt_t *const opts, const cfg_flag_t flags)
{
    return { "#title", CFGT_MVPROP, 0,       nullptr, (flags) | CFGF_TITLEPROPS, opts, 0, nullptr, false,
             0,        nullptr,     nullptr, nullptr };
}

/**
 * Initialize a flag property option.
 */
constexpr cfg_opt_t CFG_FLAG(const char *const name, const bool def, const cfg_flag_t flags)
{
    return { name, CFGT_FLAG, 0, nullptr, flags, nullptr, def, nullptr, false, 0, nullptr, nullptr, nullptr };
}

constexpr cfg_opt_t CFG_FLAG_CB(const char *const name, const bool def, const cfg_flag_t flags, const cfg_callback_t cb)
{
    return { name, CFGT_FLAG, 0, nullptr, flags, nullptr, def, nullptr, false, 0, nullptr, nullptr, cb };
}

/**
 * Terminate list of options. This must be the last initializer in
 * the option list.
 */
constexpr cfg_opt_t CFG_END()
{
    return { nullptr, CFGT_NONE, 0, nullptr, CFGF_NONE, nullptr, 0, nullptr, false, 0, nullptr, nullptr, nullptr };
}

/** Create and initialize a cfg_t structure. This should be the first
 * function called when setting up the parsing of a configuration
 * file. The options passed in the first parameter is typically
 * statically initialized, using the CFG_* initializers. The last
 * option in the option array must be CFG_END(), unless you like
 * segmentation faults.
 *
 * The options must also be defined in the same scope as where the
 * cfg_xxx functions are used. This means that you should either
 * define the option array in main(), statically in another function,
 * as global variables or dynamically using malloc(). The option array
 * is used in nearly all calls.
 *
 * @param opts An arrary of options
 * @param flags One or more flags (bitwise or'ed together)
 *
 * @return A configuration context structure. This pointer is passed
 * to all other functions as the first parameter.
 */
cfg_t *cfg_init(cfg_opt_t *opts, cfg_flag_t flags);

/** Parse a configuration file. Tilde expansion is performed on the
 * filename before it is opened. After a configuration file has been
 * initialized (with cfg_init()) and parsed (with cfg_parse()), the
 * values can be read with the cfg_getXXX functions.
 *
 * @param cfg The configuration file context as returned from cfg_init().
 * @param filename The name of the file to parse.
 *
 * @return On success, CFG_SUCCESS is returned. If the file couldn't
 * be opened for reading, CFG_FILE_ERROR is returned. On all other
 * errors, CFG_PARSE_ERROR is returned and cfg_error() was called with
 * a descriptive error message.
 */
int cfg_parse(cfg_t *cfg, const char *filename);

/** Parse a configuration lump. By James Haley.
 *
 * @param cfg The configuration file context.
 * @param lumpname The name of the lump to parse (for reference only).
 * @param lumpnum The number of the lump to parse.
 *
 * @return On success, CFG_SUCCESS is returned. If the file couldn't
 * be opened for reading, CFG_FILE_ERROR is returned. On all other
 * errors, CFG_PARSE_ERROR is returned and cfg_error() was called with
 * a descriptive error message.
 */
int cfg_parselump(cfg_t *cfg, const char *lumpname, int lumpnum);

/** Free the memory allocated for the values of a given option. Only
 * the values are freed, not the option itself (it is often statically
 * initialized).
 */
void cfg_free_value(cfg_opt_t *opt);

/** Free a cfg_t context. All memory allocated by the cfg_t context
 * structure are freed, and can't be used in any further cfg_* calls.
 */
void cfg_free(cfg_t *cfg);

/** Install a user-defined error reporting function.
 * @return The old error reporting function is returned.
 */
cfg_errfunc_t cfg_set_error_function(cfg_t *cfg, cfg_errfunc_t errfunc);

/** Install a user-defined lexer callback function.
 * @return The old lexer callback function is returned.
 */
cfg_lexfunc_t cfg_set_lexer_callback(cfg_t *cfg, cfg_lexfunc_t lexfunc);

/** Show a parser error. Any user-defined error reporting function is called.
 * @see cfg_set_error_function
 */
void cfg_error(const cfg_t *const cfg, E_FORMAT_STRING(const char *fmt), ...) E_PRINTF(2, 3);

/** Returns the value of an integer option. This is the same as
 * calling cfg_getnint with index 0.
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @return The requested value is returned. If the option was not set
 * in the configuration file, the default value given in the
 * corresponding cfg_opt_t structure is returned. If no option is found
 * with that name, 0 is returned.
 */
int cfg_getint(cfg_t *cfg, const char *name);

/** Indexed version of cfg_getint().
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param index Index of values. Zero based.
 * @see cfg_getint
 */
int cfg_getnint(cfg_t *cfg, const char *name, unsigned int index);

/** Returns the value of a floating point option.
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @return The requested value is returned. If the option was not set
 * in the configuration file, the default value given in the
 * corresponding cfg_opt_t structure is returned. If no option is found
 * with that name, cfg_error is called and 0 is returned.
 */
double cfg_getfloat(cfg_t *cfg, const char *name);

/** Returns the value of a string option.
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @return The requested value is returned. If the option was not set
 * in the configuration file, the default value given in the
 * corresponding cfg_opt_t structure is returned. If no option is found
 * with that name, cfg_error is called and nullptr is returned.
 */
const char *cfg_getstr(cfg_t *cfg, const char *name);

/** Returns a heap-allocated duplicate of a string option.
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @return A heap-allocated copy of the value is returned. If the option
 * was not set in the configuration file, the default value given in the
 * corresponding cfg_opt_t structure is returned. If no option is found
 * with that name, cfg_error is called and nullptr is returned.
 */
char *cfg_getstrdup(cfg_t *cfg, const char *name);

/** Returns the value of a boolean option.
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @return The requested value is returned. If the option was not set
 * in the configuration file, the default value given in the
 * corresponding cfg_opt_t structure is returned. If no option is found
 * with that name, cfg_error is called and false is returned.
 */
bool cfg_getbool(cfg_t *cfg, const char *name);

/** Returns the value of a section option. The returned value is
 * another cfg_t structure that can be used in following calls to
 * cfg_getint, cfg_getstr or other get-functions.
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @return The requested value is returned.  If no section is found
 * with that name, 0 is returned. Note that there can be no default
 * values for a section.
 */
cfg_t *cfg_getsec(const cfg_t *const cfg, const char *name);

/** Returns the value of a multi-valued property. The returned value is
 * another cfg_t structure that can be used in following calls to
 * cfg_getint, cfg_getstr, or other get-functions.
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @return The requested value is returned. If no property is found
 * with that name, 0 is returned. Note that there can be no default
 * values for a multi-valued property.
 */
cfg_t *cfg_getmvprop(cfg_t *cfg, const char *name);

/** Returns the value of a flag option. This is the same as
 * calling cfg_getnflag with index 0.
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @return The requested value is returned. If the option was not set
 * in the configuration file, the default value given in the
 * corresponding cfg_opt_t structure is returned. If no option is found
 * with that name, 0 is returned.
 */
int cfg_getflag(cfg_t *cfg, const char *name);

/** Indexed version of cfg_getflag().
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param index Index of values. Zero based.
 * @see cfg_getflag
 */
int cfg_getnflag(cfg_t *cfg, const char *name, unsigned int index);

cfg_value_t *cfg_setopt(cfg_t *cfg, cfg_opt_t *opt, const char *value);

/** Return the number of values this option has. If the option does
 * not have the CFGF_LIST or CFGF_MULTI flag set, this function will
 * always return 1.
 * @param cfg The configuration file context.
 * @param name The name of the option.
 */
unsigned int cfg_size(const cfg_t *const cfg, const char *name);

/** Indexed version of cfg_getfloat().
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param index Index of values. Zero based.
 * @see cfg_getint
 */
double cfg_getnfloat(cfg_t *cfg, const char *name, unsigned int index);

/** Indexed version of cfg_getstr().
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param index Index of values. Zero based.
 * @see cfg_getstr
 */
const char *cfg_getnstr(cfg_t *cfg, const char *name, unsigned int index);

/** Indexed version of cfg_getbool().
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param index Index of values. Zero based.
 * @see cfg_getstr
 */
bool cfg_getnbool(cfg_t *cfg, const char *name, unsigned int index);

/** Indexed version of cfg_getsec().
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param index Index of values. Zero based.
 * @see cfg_getsec
 */
cfg_t *cfg_getnsec(const cfg_t *const cfg, const char *name, unsigned int index);

/** Return a section given the title.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param title The title of this section. The CFGF_TITLE flag must
 * have been set for this option.
 */
cfg_t *cfg_gettsec(cfg_t *cfg, const char *name, const char *title);

/** Return the title of a section.
 *
 * @param cfg The configuration file context.
 * @return Returns the title, or 0 if there is no title. This string
 * should not be modified.
 */
const char *cfg_title(cfg_t *cfg);

/**
 * Return a displaced section. A displaced section occurs when more than
 * one section in the same option with the same name has been defined.
 * @param cfg The configuration file context.
 * @return Returns a displaced section, or 0 if there is none.
 */
cfg_t *cfg_displaced(cfg_t *cfg);

/**
 * Indexed version of cfg_getmvprop().
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param index Index of values. Zero based.
 * @see cfg_getsec
 */
cfg_t *cfg_getnmvprop(cfg_t *cfg, const char *name, unsigned int index);

/**
 * Return a pointer to the anonymous multi-valued property that is flagged
 * as CFGF_TITLEPROPS inside the given cfg_t. This MVPROP holds the values of
 * any options that are specified following a colon after the section's title.
 * @param cfg The configuration file context.
 * @return Context holding the title property values.
 * @see cfg_getmvprop
 */
cfg_t *cfg_gettitleprops(cfg_t *cfg);

extern const char *confuse_copyright;
extern const char *confuse_version;
extern const char *confuse_author;

/** Predefined include-function. This function can be used in the
 * options passed to cfg_init() to specify a function for including
 * other configuration files in the parsing. For example:
 * CFG_FUNC("include", &cfg_include)
 */
int cfg_include(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv);

char *cfg_tilde_expand(const char *filename);

/** Parse a boolean option string. Accepted "true" values are "true",
 * "on" and "yes", and accepted "false" values are "false", "off" and
 * "no".
 *
 * @return Returns 1 or 0 (true/false) if the string was parsed
 * correctly, or -1 if an error occurred.
 */
int cfg_parse_boolean(const char *s);

/** Return an option given it's name.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 *
 * @return Returns a pointer to the option, or nullptr if the option is
 * not found (an error message is also printed).
 */
cfg_opt_t *cfg_getopt(const cfg_t *const cfg, const char *name);

/** Set a value of an integer option.
 *
 * @param cfg The configuration file context.
 * @param opt The option structure (eg, as returned from cfg_getopt())
 * @param value The value to set.
 * @param index The index in the option value array that should be
 * modified. It is an error to set values with indices larger than 0
 * for options without the CFGF_LIST flag set.
 */
void cfg_opt_setnint(cfg_t *cfg, cfg_opt_t *opt, int value, unsigned int index);

/** Set a value of an integer option given its name.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param value The value to set.
 * @param index The index in the option value array that should be
 * modified. It is an error to set values with indices larger than 0
 * for options without the CFGF_LIST flag set.
 */
void cfg_setnint(cfg_t *cfg, const char *name, int value, unsigned int index);

/** Set a value of an integer option given its name.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param value The value to set.
 */
void cfg_setint(cfg_t *cfg, const char *name, int value);

/** Set a value of an floating point option.
 *
 * @param cfg The configuration file context.
 * @param opt The option structure (eg, as returned from cfg_getopt())
 * @param value The value to set.
 * @param index The index in the option value array that should be
 * modified. It is an error to set values with indices larger than 0
 * for options without the CFGF_LIST flag set.
 */
void cfg_opt_setnfloat(cfg_t *cfg, cfg_opt_t *opt, double value, unsigned int index);

/** Set a value of a floating point option given its name.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param value The value to set.
 * @param index The index in the option value array that should be
 * modified. It is an error to set values with indices larger than 0
 * for options without the CFGF_LIST flag set.
 */
void cfg_setnfloat(cfg_t *cfg, const char *name, double value, unsigned int index);

/** Set a value of a floating point option given its name.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param value The value to set.
 */
void cfg_setfloat(cfg_t *cfg, const char *name, double value);

/** Set a value of a boolean option.
 *
 * @param cfg The configuration file context.
 * @param opt The option structure (eg, as returned from cfg_getopt())
 * @param value The value to set.
 * @param index The index in the option value array that should be
 * modified. It is an error to set values with indices larger than 0
 * for options without the CFGF_LIST flag set.
 */
void cfg_opt_setnbool(cfg_t *cfg, cfg_opt_t *opt, bool value, unsigned int index);

/** Set a value of a boolean option given its name.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param value The value to set.
 * @param index The index in the option value array that should be
 * modified. It is an error to set values with indices larger than 0
 * for options without the CFGF_LIST flag set.
 */
void cfg_setnbool(cfg_t *cfg, const char *name, bool value, unsigned int index);

/** Set a value of a boolean option given its name.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param value The value to set.
 */
void cfg_setbool(cfg_t *cfg, const char *name, bool value);

/** Set a value of a string option.
 *
 * @param cfg The configuration file context.
 * @param opt The option structure (eg, as returned from cfg_getopt())
 * @param value The value to set.
 * @param index The index in the option value array that should be
 * modified. It is an error to set values with indices larger than 0
 * for options without the CFGF_LIST flag set.
 */
void cfg_opt_setnstr(cfg_t *cfg, cfg_opt_t *opt, const char *value, unsigned int index);

/** Set a value of a string option given its name.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param value The value to set.
 * @param index The index in the option value array that should be
 * modified. It is an error to set values with indices larger than 0
 * for options without the CFGF_LIST flag set.
 */
void cfg_setnstr(cfg_t *cfg, const char *name, const char *value, unsigned int index);

/** Set a value of a string option given its name.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param value The value to set.
 */
void cfg_setstr(cfg_t *cfg, const char *name, const char *value);

/** Set a list of values, deleting any values previously in the list.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param nvalues The number of values provided as additional arguments.
 */
void cfg_setlist(cfg_t *cfg, const char *name, unsigned int nvalues, ...);

/** Set a list of values, adding the new values to the end of the list.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param nvalues The number of values provided as additional arguments.
 */
void cfg_addlist(cfg_t *cfg, const char *name, unsigned int nvalues, ...);

/** Set a list of values from an array.
 *
 * @param cfg The configuration file context.
 * @param name The name of the option.
 * @param nvalues The number of values in the array.
 * @param array Pointer to an array of the appropriate type cast to void pointer.
 */
void cfg_setlistptr(cfg_t *cfg, const char *name, unsigned int nvalues, const void *valarray);
#endif

/** @example cfgtest.c
 */

/** @example simple.c
 */

/** @example yafc_cfg.c
 */
