/*

   RWOps - File IO Abstraction Support

   jhaley 20120705

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rwops.h"

//=============================================================================
//
// Globals
//

// Callbacks into application
static rwopen_t   open_callback;
static rwcclose_t close_callback;

//=============================================================================
//
// Read from file
//

//
// stdio_seek
//
// This doubles as "tell"
//
static long stdio_seek(RWops_t *context, long offset, int whence)
{
   return (fseek(context->fp, offset, whence) == 0) ? ftell(context->fp) : -1;
}

//
// stdio_read
//
static size_t stdio_read(RWops_t *context, void *ptr, size_t size, size_t maxnum)
{
   return fread(ptr, size, maxnum, context->fp);
}

//
// stdio_gets
//
static char *stdio_gets(char *str, int num, RWops_t *context)
{
   return fgets(str, num, context->fp);
}

//
// stdio_write
//
static size_t stdio_write(RWops_t *context, const void *ptr, size_t size, size_t num)
{
   return fwrite(ptr, size, num, context->fp);
}

//
// stdio_close
//
static int stdio_close(RWops_t *context)
{
   int result = 0;

   if(context && !context->staticHandle)
   {
      if(context->fp)
         result = fclose(context->fp);
      
      free(context);
   }

   return result;
}

//
// stdio_error
//
static int stdio_error(RWops_t *context)
{
   return ferror(context->fp);
}

//=============================================================================
//
// Read from memory
//

//
// memory_seek
//
// This doubles as "tell"
//
static long memory_seek(RWops_t *context, long offset, int whence)
{
   rwbyte *newpos;

   switch(whence)
   {
   case SEEK_SET:
      newpos = context->base + offset;
      break;
   case SEEK_CUR:
      newpos = context->here + offset;
      break;
   case SEEK_END:
      newpos = context->stop + offset;
      break;
   default:
      return -1; // error
   }

   if(newpos < context->base)
      newpos = context->base;
   if(newpos > context->stop)
      newpos = context->stop;
   context->here = newpos;

   return (long)(context->here - context->base);
}

//
// memory_read
//
static size_t memory_read(RWops_t *context, void *ptr, size_t size, size_t maxnum)
{
   size_t total_bytes;
   size_t mem_available;

   total_bytes = (maxnum * size);

   if(maxnum <= 0 || size <= 0 || (total_bytes / maxnum) != size)
      return 0;

   mem_available = (size_t)(context->stop - context->here);
   if(total_bytes > mem_available)
      total_bytes = mem_available;

   memcpy(ptr, context->here, total_bytes);
   context->here += total_bytes;

   return (total_bytes / size);
}

//
// memory_gets
//
static char *memory_gets(char *str, int num, RWops_t *context)
{
   char c = 0;
   char *cs;

   cs = str;
   while(--num > 0 && context->read(context, &c, 1, 1) == 1)
   {
      if(c == '\r') // emulate text mode; pretend \r's don't exist
      {
         ++num;
         continue;
      }

      *cs++ = c;
      if(c == '\n')
         break;
   }

   if(context->here >= context->stop && cs == str)
      return NULL;

   *cs = '\0';
   return str;
}

//
// memory_write
//
static size_t memory_write(RWops_t *context, const void *ptr, size_t size, size_t num)
{
   if(context->here + (num*size) > context->stop)
      num = (context->stop - context->here) / size;

   memcpy(context->here, ptr, num*size);
   context->here += num*size;

   return num;
}

//
// memory_close
//
static int memory_close(RWops_t *context)
{
   if(context)
   {
      // Application may want to free the memory referred to by context->base
      if(close_callback && context->base)
         close_callback(context->base);

      free(context);
   }
   
   return 0;
}

//
// memory_error
//
static int memory_error(RWops_t *context)
{
   return 0; // ???
}

//=============================================================================
//
// Interface
//

static RWops_t *mcpp_allocrw()
{
   return (RWops_t *)(malloc(sizeof(RWops_t)));
}

//
// mcpp_setopencallback
//
// DLL Export.
// Set the application's fopen callback.
//
void mcpp_setopencallback(rwopen_t oc)
{
   open_callback = oc;
}

//
// mcpp_setclosecallback
//
// DLL Export.
// Set the application's fclose callback.
//
void mcpp_setclosecallback(rwcclose_t cc)
{
   close_callback = cc;
}

//
// mcpp_openmemory
//
// DLL Export.
// Create an RWops_t that can read from memory.
//
void *mcpp_openmemory(void *mem, size_t size)
{
   RWops_t *rw = mcpp_allocrw();

   if(!rw)
      return NULL;

   rw->seek  = memory_seek;
   rw->read  = memory_read;
   rw->gets  = memory_gets;
   rw->write = memory_write;
   rw->close = memory_close;
   rw->error = memory_error;
   rw->base  = (rwbyte *)mem;
   rw->here  = rw->base;
   rw->stop  = rw->base + size;

   return rw;
}

//
// mcpp_openfile
//
// DLL Export.
// Create an RWops_t that can read from file.
//
void *mcpp_openfile(const char *file, const char *mode)
{
   RWops_t *rw = mcpp_allocrw();
   FILE *fp = NULL;

   if(!rw)
      return NULL;

   fp = fopen(file, mode);
   if(!fp)
   {
      free(rw);
      return NULL;
   }

   rw->seek  = stdio_seek;
   rw->read  = stdio_read;
   rw->gets  = stdio_gets;
   rw->write = stdio_write;
   rw->close = stdio_close;
   rw->error = stdio_error;
   rw->fp    = fp;
   
   rw->staticHandle = 0;

   return rw;
}

//
// mcpp_openstdin
//
// Application local, not an export.
// Create a file RWops that reads from stdin.
//
RWops_t *mcpp_openstdin(void)
{
   static int firsttime = 1;
   static RWops_t stdin_rwops;

   if(firsttime)
   {
      stdin_rwops.seek  = stdio_seek;
      stdin_rwops.read  = stdio_read;
      stdin_rwops.gets  = stdio_gets;
      stdin_rwops.write = stdio_write;
      stdin_rwops.close = stdio_close;
      stdin_rwops.error = stdio_error;
      stdin_rwops.fp    = stdin;
      
      stdin_rwops.staticHandle = 1;

      firsttime = 0;
   }

   return &stdin_rwops;
}

//
// mcpp_fopen
//
// Application local, not an export.
// The library calls this to open a file.
//
RWops_t *mcpp_fopen(const char *file, const char *mode)
{
   RWops_t *rw = NULL;
   
   if(open_callback)
   {
      // Callback into the application.
      // It will return a value created by one of the above functions.
      rw = (RWops_t *)(open_callback(file, mode));
   }
   else
      rw = mcpp_openfile(file, mode); // default to stdio

   return rw;
}

// EOF

