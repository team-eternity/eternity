# Eternity Engine Programming Style Guide

## Coding style guidelines

The coding style guidelines for Eternity are designed to keep the style 
consistent throughout the program. Some of these guidelines are stricter than
what was done in the original source; follow these as much as possible, and
reformat any old code you edit to match the new style guidelines.

Remove all tabs. Set Visual C++, if you use it, to "Replace tabs with spaces,"
and set indentation to three (3). Remove any tabs you might find either
manually or, in Visual C++, by selecting the offending text and hitting ALT+F8.
This auto-formats the selection (but be careful, it has some bugs, especially
with `?:` statements, array/struct initializers, and with blocks longer than
1000 lines).

Please write code to a 95 column limit so that it fits on a 1680x1050 monitor
when using a reasonable font size.

Global functions should be named like this: `AB_FunctionName`.  The `AB` prefix 
denotes the subsystem (`AM_` for automap, `G_` for game, etc).  If a function is 
static, you can omit the prefix and just name it like `FunctionName`, or
preferably, use the prefix and start the proper function name with a lower-case
letter (ex: `AB_functionName`).  Functions and global variables should always be 
made static unless they are required in another module. Inline functions must
be declared as static also, or they will cause linker errors if the compiler
target does not support inlining.

It is suggested but not required to put `_t` on the end of structure names and
types created with `typedef`.  An example of this is `txt_window_t`.

Do not use Hungarian notation, except when dealing with confusing levels of
indirection. It is sometimes helpful, for instance, to prefix pointers with
the letter p (or pp with pointers to pointers).

Prefer C++-style comments, ie. `//` comments over `/* ... */` comments. The
latter are best used only for extremely long comment blocks, especially if
example code is contained in the comment, or for temporarily removing dead
or experimental code. `#if 0` may be preferable for the latter, however.

In pointer variable declarations, place the `*` next to the variable name, not 
the type.

When using an if, do, while, or for statement, always use the { } braces if the
code block is more than one line long, even if this is only because of a 
comment:
```C++
   if(condition)
   {
      // hello
      body;
   }
```

Write code like this:

```C++
struct my_structure_t
{
   int   member1; // comment every structure member
   char *member2; // IDEs use these comments to help you
};

static my_structure_t array[] =
{
   {  10, "hello" }, // space elements in structures for beautification.
   { 200, "world" },
};

class ClassesAreCapitalized : public ParentClass
{
   // This is first if the type has RTTI
   DECLARE_RTTI_TYPE(ClassesAreCapitalized, ParentClass)
   
   // private declarations are first. Use pImpl idioms if the contents would
   // require inclusion of heavy template components such as EHashTable.
private:
   int i;
   
   // Protected components come next
protected:
   void calculate() { i = i*2; }
   
   // Public components come last
public:
   // Public constructors and destructors are first:
   ClassesAreCapitalized()
      : Super(), i(0)  // initializers in declaration order
   {
   }
   
   //
   // Copy constructor
   //
   ClassesAreCapitalized(const ClassesAreCapitalized &other)
      : Super(), i(other.i)
   {
   }
   
   // Methods that are virtual in the base class should be reiterated 
   // as such in descendants for clarity even though it is not required.
   virtual ~ClassesAreCapitalized() {}
   
   //
   // Accessors
   //
   // If there would be both get and set with no semantics, consider if the member
   // should be public. This is not forbidden when it makes sense for efficiency's sake.
   //
   
   int getI() const { return i; }
};

// If template declarations are verbose, put them on the line above the declarator:
template<typename LongTypeNameA, typename LongTypeNameB>
LongTypeNameA template_function(A a, B b)
{
   return a + b;
}

// Lambdas should use K&R bracket style
auto foo = [=x] () {
  return x + 1;
};

// And here is one reason why:
// A lambda called in place would look as such (though unlikely to occur):
([] (int x) {
   printf("%d\n", x);
})(100);

// And another, when passing lambdas as callbacks:
std::sort(arr.begin(), arr.end(), [&foo] (const A &a, const B &b) {
   return a < b;
});

// enums:

enum ordinals_e
{
   FIRST  = 1, // enum symbols should usually be allcaps
   SECOND = 2  // last item should NOT have a comma because it is not standard compliant
};

// long strings -- use consecutive string literal concatenation; NEVER use the
// obsolete line continuation symbol (ie: \)
// ALWAYS make pointers to character constants "const" -- if you don't and they
// get written into, I will not cry tears for you when it crashes :P

static const char *long_str = "This string is too long to fit on one line so "
                              "I'm going to break it across these two lines.";

//
// Functions should have header comments that describe the function's purpose,
// arguments, and return value. IDEs use these comments to help you, and they
// also make me really happy when it comes time to edit the code.
//
// Functions which take no arguments should have an empty lozenge (), not
// (void) as this is unnecessarily verbose.
//
void A_FunctionName(int argument, int arg2, int arg3, int arg4, int arg5, int arg6, 
                    int arg7)
{
   // put comments here...
   if(condition)  // or here...
   { // but NEVER here. this makes the code hard to read. I HATE this.
      body;
   }
   else if(condition) // NO spaces between keywords and parens. Brrrrr >_<
   {
      // comments inside the block are fine, but they must share the block's
      // indentation level
      body;
   }
   else 
      body;
      
#if 0  // with a few exceptions, preprocessor directives don't indent
   CallCrashyCode();
#endif

   if(very_long_condition_like_this_one_that_forces_a_line_break && 
      other_condition)
   {
      // operators: space between all operators and operands. Do not cram up
      // mathematical expressions just to save space, as this makes them
      // unreadable.
      int x = 0, y = 1, z = 2, temp;
      
      x = y * z + 4;
      
      // parens: NO space between parens and operands. I hate that, unless it's
      // being used to attain better alignment in multiline expressions.
      x = y * (z + 4);
      
      // an example of how to align for beautification -- make columns.
      temp  =        *demo_p++;         // byte one
      temp |= ((int)(*demo_p++)) <<  8; // byte two
      temp |= ((int)(*demo_p++)) << 16; // byte three
      temp |= ((int)(*demo_p++)) << 24; // byte four
      
      // if you must break a line within a single expression, alter the level of
      // indentation to be consistent with the expression depth:
      
      x = (really_long_variable_from_some_other_file + 
           really_long_variable_from_another_file) * x;
             
      // if indenting arguments to a function call, line them up. Exceptions may
      // be made for very long format strings, etc. The closing paren and semi-
      // colon go on the line with the last argument unless they won't fit 
      // there, in which case, line them up with the opening paren.
      
      y = TweakVarsFromOtherFiles(&really_long_variable_from_some_other_file,
                                  &really_long_variable_from_another_file);
   }

   // ^ keep at least one line of space between distinct control blocks, but no
   // more than two. Too much is as bad as not enough.
   switch(argument)
   {
   case FIRST: // cases line up with the opening brace for the switch
      code;    // case code is indented one level.
      break;   // keep comments vertically aligned for beautification.
            
   case SECOND:
      code;
      break;
            
   default:
      break;   // a statement before the end of a switch is required
   }

   // space between operators and operands, but not between operands and
   // semicolons. Prefer a++ to ++a; I will change it if I see it. Exceptions
   // are made for C++ objects with non-POD iterator types (which does NOT
   // include PODCollection/Collection)
   for(a = 0; a < 10; a++)
   {
      loop_body;
   }

   while(a < 10)
   {
      loop_body;
   }

   do 
   {
      body;
   }
   while(condition); // put the while down one line; it looks better.
}
```

## GNU GPL and licensing

All code submitted to the project must be licensed under the GNU GPLv3 or a
compatible license.  If you use code that you haven't 100% written 
yourself, say so. Add a copyright header to the start of every file.  Use
this template:

```C
//
// The Eternity Engine
// Copyright(C) YEAR James Haley, (your name here), et al.
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
// Purpose: (basic description of the module here)
// Authors: (your name here), any other people
//
```

If the file contains code that only Team Eternity members have contributed, add
this additional blurb after the GPLv3 text:

```C++
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
```

## Other Stuff

All files (excepting old ones with CVS logs) should end with:

```C
// EOF\n
\n
```

Where the `\n` are literal linebreaks (don't type `\n`, just hit enter). Doing 
this ensures that all files are terminated with a linebreak. Never put anything
else after the EOF comment, but move it down when you edit at the bottom of a 
file.

Includes:
Include as few headers as you can get away with. Use this syntax:

`#include "foo.h"`

`<>` are for system includes only.

Inside include files, the first thing after the header comment should be this:
```C++
#ifndef A_FILENAME_H__
#define A_FILENAME_H__

// contents...

#endif
```

### Order within a file

Try to keep related code together. Write functions "bottom-up", ie if a static
function is called by another, it should be written above the calling function 
in the file. Declare most if not all global and static global variables at the
top of the file just after include statements, and comment what is global and
what is private to the module. If used by the whole file, also make macros,
enums, etc. sections of their own at the top. Otherwise, keep them directly
above the function that uses them.


