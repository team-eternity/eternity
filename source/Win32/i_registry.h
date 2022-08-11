// Do not define EE_FEATURE_REGISTRYSCAN unless you are using a compatible
// compiler.

#ifndef __I_REGISTRY_H__
#define __I_REGISTRY_H__

#ifdef EE_FEATURE_REGISTRYSCAN

#include "z_zone.h"
#include "z_auto.h"

#include "../m_qstr.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if _WIN64
#define SOFTWARE_KEY "Software\\Wow6432Node"
#else
#define SOFTWARE_KEY "Software"
#endif

//
// Static data on a registry key to be read.
//
struct registry_value_t
{
   HKEY root;         // registry root (ie. HKEY_LOCAL_MACHINE)
   const char *path;  // registry path
   const char *value; // registry key
};

//
// This class represents an open registry key. The key handle will be closed
// when an instance of this class falls out of scope.
//
class AutoRegKey
{
protected:
   HKEY key;
   bool valid;

public:
   // Constructor. Open the key described by rval.
   explicit AutoRegKey(const registry_value_t &rval) : valid(false)
   {
      if(!RegOpenKeyEx(rval.root, rval.path, 0, KEY_READ, &key))
         valid = true;
   }

   // Disallow copying
   AutoRegKey(const AutoRegKey &) = delete;
   AutoRegKey &operator = (const AutoRegKey &) = delete;

   // Destructor. Close the key handle, if it is valid.
   ~AutoRegKey()
   {
      if(valid)
      {
         RegCloseKey(key);
         key   = nullptr;
         valid = false;
      }
   }

   // Test if the key is open.
   bool operator ! () const { return !valid; }

   // Query the key value using the Win32 API RegQueryValueEx.
   LONG queryValueEx(LPCTSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                     LPBYTE lpData, LPDWORD lpcbData)
   {
      return RegQueryValueEx(key, lpValueName, lpReserved, lpType, lpData, lpcbData);
   }
};

//
// Open the registry key described by regval and retrieve its value, assuming
// it is a REG_SZ (null-terminated string value). If successful, the results
// are stored in qstring "str". Otherwise, the qstring is unmodified.
//
static bool I_GetRegistryString(const registry_value_t &regval, qstring &str)
{
   DWORD  len;
   DWORD  valtype;
   LPBYTE result;

   // Open the key (directory where the value is stored)
   AutoRegKey key(regval);
   if(!key)
      return false;

   // Find the type and length of the string
   if(key.queryValueEx(regval.value, nullptr, &valtype, nullptr, &len))
      return false;

   // Only accept strings
   if(valtype != REG_SZ)
      return false;

   // Allocate a buffer for the value and read the value
   ZAutoBuffer buffer(len, true);
   result = buffer.getAs<LPBYTE>();

   if(key.queryValueEx(regval.value, nullptr, &valtype, result, &len))
      return false;

   str = reinterpret_cast<char *>(result);

   return true;
}

#endif // defined(EE_FEATURE_REGISTRYSCAN)

#endif // defined(__I_REGISTRY_H__)

// EOF

