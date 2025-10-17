//
// The Eternity Engine
// Copyright (C) 2025 James Haley
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
//------------------------------------------------------------------------------
//
// Purpose: Exception handling for Windows, because the SDL parachute sucks.
//  Based loosely on ExceptionHandler.cpp
//  Author:  Hans Dietrich
//           hdietrich2@hotmail.com
//  Original license: public domain
//
// Authors: Hans Dietrich, James Haley, Max Waine
//

#if defined(_MSC_VER)

#include <windows.h>
#include <tchar.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4091)
#endif

#include <dbghelp.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <atomic>

#include "../d_keywds.h"
#include "../version.h"

//=============================================================================
//
// Macros
//

#define charcount(arr) (sizeof(arr) / sizeof((arr)[0]))
#define LOG_BUFFER_SIZE 8192
#define MEGABYTE (1024*1024)
#define RoundMem(amt) (((amt) + MEGABYTE - 1) / MEGABYTE)
#define CODETOPRINT 16
#define STACKTOPRINT 3072

//=============================================================================
//
// Data
//

static std::atomic_bool exceptionHandled;

static wchar_t  moduleFileName[MAX_PATH * 2];
static wchar_t  moduleName[MAX_PATH * 2];
static wchar_t  crashModulePath[MAX_PATH * 2];
static HANDLE   logFile;
static TCHAR    logbuffer[LOG_BUFFER_SIZE];
static int      logidx;
static wchar_t *fileName;

static PEXCEPTION_RECORD exceptionRecord;
static PCONTEXT          contextRecord;

static NTSTATUS(WINAPI *RtlGetVersion)(LPOSVERSIONINFOEXW);
static OSVERSIONINFOEXW rtlver;

//=============================================================================
//
// Static Routines
//

//
// lstrrchr
//
static TCHAR *lstrrchr(LPCTSTR str, int ch)
{
    TCHAR *start = (TCHAR *)str;

    while(*str++)
        ; // find end

    // search backward
    while(--str != start && *str != (TCHAR)ch)
        ;

    if(*str == (TCHAR)ch)
        return (TCHAR *)str; // found it

    return NULL;
}

//
// ExtractFileName
//
// Gets the filename from a path string
//
static TCHAR *ExtractFileName(LPCTSTR path)
{
    TCHAR *ret;

    // look for last instance of a backslash
    if((ret = lstrrchr(path, _T('\\'))))
        ++ret;
    else
        ret = (TCHAR *)path;

    return ret;
}
static wchar_t *ExtractFileName(wchar_t *path)
{
    wchar_t *ret;
    if((ret = wcsrchr(path, L'\\')))
        ++ret;
    else
        ret = path;
    return ret;
}

//
// GetModuleName
//
// Gets the module file name as a full path, and extracts the file portion.
//
static void GetModuleName(void)
{
    wchar_t *dot;

    ZeroMemory(moduleFileName, sizeof(moduleFileName));

    if(GetModuleFileNameW(NULL, moduleFileName, charcount(moduleFileName) - 2) <= 0)
        wcscpy_s(moduleFileName, L"Unknown");

    fileName = ExtractFileName(moduleFileName);
    wcscpy_s(moduleName, fileName);

    // find extension and remove it
    if((dot = wcsrchr(moduleName, L'.')))
        dot[0] = 0;

    // put filename onto module path
    wcscpy_s(fileName, charcount(moduleName) - (fileName - moduleName), L"CRASHLOG.TXT");
}

//=============================================================================
//
// Log File Stuff
//

//
// OpenLogFile
//
// Opens the exception log file.
//
static int OpenLogFile(void)
{
    logFile = CreateFileW(moduleFileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, 0);

    if(logFile == INVALID_HANDLE_VALUE)
        return 0;
    else
    {
        // set to append
        SetFilePointer(logFile, 0, 0, FILE_END);
        return 1;
    }
}

//
// LogFlush
//
// To avoid too many WriteFile calls, we'll buffer the output.
//
static void LogFlush(HANDLE file)
{
    DWORD bytecount;

    if(logidx > 0)
    {
        WriteFile(file, logbuffer, lstrlen(logbuffer), &bytecount, 0);
        logidx = 0;
    }
}

//
// LogPrintf
//
// This is supposed to be safe to call during an exception, unlike fprintf.
// We'll see.
//
static void LogPrintf(LPCTSTR fmt, ...)
{
    DWORD   bytecount;
    va_list args;

    va_start(args, fmt);

    if(logidx > LOG_BUFFER_SIZE - 1024)
    {
        WriteFile(logFile, logbuffer, lstrlen(logbuffer), &bytecount, 0);
        logidx = 0;
    }

    logidx += wvsprintf(&logbuffer[logidx], fmt, args);

    va_end(args);
}

//=============================================================================
//
// Information Output Routines
//

struct exceptiondata_t
{
    DWORD        code;
    const TCHAR *name;
};

static exceptiondata_t ExceptionData[] = {
    { 0x40010005, _T("a Ctrl+C")                     },
    { 0x40010008, _T("a Ctrl+Brk")                   },
    { 0x80000002, _T("a Data Type Misalignment")     },
    { 0x80000003, _T("a Breakpoint")                 },
    { 0xC0000005, _T("an Access Violation")          },
    { 0xC0000006, _T("an In-Page Error")             },
    { 0xC0000017, _T("a No Memory")                  },
    { 0xC000001D, _T("an Illegal Instruction")       },
    { 0xC0000025, _T("a Non-continuable Exception")  },
    { 0xC0000026, _T("an Invalid Disposition")       },
    { 0xC000008C, _T("an Array Bounds Exceeded")     },
    { 0xC000008D, _T("a Float Denormal Operand")     },
    { 0xC000008E, _T("a Float Divide by Zero")       },
    { 0xC000008F, _T("a Float Inexact Result")       },
    { 0xC0000090, _T("a Float Invalid Operation")    },
    { 0xC0000091, _T("a Float Overflow")             },
    { 0xC0000092, _T("a Float Stack Check")          },
    { 0xC0000093, _T("a Float Underflow")            },
    { 0xC0000094, _T("an Integer Divide by Zero")    },
    { 0xC0000095, _T("an Integer Overflow")          },
    { 0xC0000096, _T("a Privileged Instruction")     },
    { 0xC00000FD, _T("a Stack Overflow")             },
    { 0xC0000142, _T("a DLL Initialization Failure") },
    { 0xE06D7363, _T("a Microsoft C++")              },

    // must be last
    { 0,          NULL                               }
};

//
// PhraseForException
//
// Gets a text string describing the type of exception that occurred.
//
static const TCHAR *PhraseForException(DWORD code)
{
    const TCHAR     *ret = _T("an Unknown");
    exceptiondata_t *ed  = ExceptionData;

    while(ed->name)
    {
        if(code == ed->code)
        {
            ret = ed->name;
            break;
        }
        ++ed;
    }

    return ret;
}

//
// PrintHeader
//
// Prints a header for the exception.
//
static void PrintHeader(void)
{
    const wchar_t           *crashModuleFn = L"Unknown";
    MEMORY_BASIC_INFORMATION memoryInfo;

    ZeroMemory(crashModulePath, sizeof(crashModulePath));

#ifdef _M_IX86
    // Use VirtualQuery to retrieve the allocation base associated with the
    // process's code address.
    if(VirtualQuery((void *)contextRecord->Eip, &memoryInfo, sizeof(memoryInfo)))
    {
        if(GetModuleFileName((HINSTANCE)memoryInfo.AllocationBase, crashModulePath, charcount(crashModulePath) - 2) > 0)
        {
            crashModuleFn = ExtractFileName(crashModulePath);
        }
    }

    LogPrintf(_T("%s caused %s Exception (0x%08x)\r\nin module %s at %04x:%08x.\r\n\r\n"), moduleName,
              PhraseForException(exceptionRecord->ExceptionCode), exceptionRecord->ExceptionCode, crashModuleFn,
              contextRecord->SegCs, contextRecord->Eip);
#else
    // Use VirtualQuery to retrieve the allocation base associated with the
    // process's code address.
    if(VirtualQuery((void *)contextRecord->Rip, &memoryInfo, sizeof(memoryInfo)))
    {
        if(GetModuleFileNameW((HINSTANCE)memoryInfo.AllocationBase, crashModulePath, charcount(crashModulePath) - 2) >
           0)
        {
            crashModuleFn = ExtractFileName(crashModulePath);
        }
    }

    LogPrintf(_T("IMPORTANT!: When submitting a crashlog, DO NOT modify or remove any part of this file.\r\n"));

    LogPrintf(_T("%s caused %s Exception (0x%08x)\r\nin module %s at %04x:%016I64x\r\n\r\n"), moduleName,
              PhraseForException(exceptionRecord->ExceptionCode), exceptionRecord->ExceptionCode, crashModuleFn,
              contextRecord->SegCs, contextRecord->Rip);
#endif

    LogPrintf(_T("Engine version: %s\r\nDate compiled: " __DATE__ "\r\n\r\n"), ee_wmCaption);
}

//
// MakeTimeString
//
static void MakeTimeString(FILETIME time, LPTSTR str)
{
    WORD d, t;

    str[0] = _T('\0');

    if(FileTimeToLocalFileTime(&time, &time) && FileTimeToDosDateTime(&time, &d, &t))
    {
        wsprintf(str, _T("%d/%d/%d %02d:%02d:%02d"), (d / 32) & 15, d & 31, d / 512 + 1980, t >> 11, (t >> 5) & 0x3F,
                 (t & 0x1F) * 2);
    }
}

//
// PrintTime
//
// Prints the time at which the exception occurred.
//
static void PrintTime(void)
{
    FILETIME crashtime;
    TCHAR    timestr[256];

    GetSystemTimeAsFileTime(&crashtime);
    MakeTimeString(crashtime, timestr);

    LogPrintf(_T("Error occurred at %s.\r\n"), timestr);
}

//
// PrintUserInfo
//
// Prints out info on the user running the process.
//
static void PrintUserInfo(void)
{
    TCHAR moduleName[MAX_PATH * 2];
    TCHAR userName[256];
    DWORD userNameLen;

    ZeroMemory(moduleName, sizeof(moduleName));
    ZeroMemory(userName, sizeof(userName));
    userNameLen = charcount(userName) - 2;

    if(GetModuleFileName(0, moduleName, charcount(moduleName) - 2) <= 0)
        lstrcpy(moduleName, _T("Unknown"));

    if(!GetUserName(userName, &userNameLen))
        lstrcpy(userName, _T("Unknown"));

    LogPrintf(_T("%s, run by %s.\r\n"), moduleName, userName);
}

//
// PrintOSInfo
//
// Prints out version information for the operating system.
//
static void PrintOSInfo(void)
{
    TCHAR mmb[64];

    ZeroMemory(mmb, sizeof(mmb));

    if(RtlGetVersion)
    {
        rtlver.dwOSVersionInfoSize = sizeof(rtlver);
        RtlGetVersion(&rtlver);

        DWORD minorVersion = rtlver.dwMinorVersion;
        DWORD majorVersion = rtlver.dwMajorVersion;
        DWORD buildNumber  = rtlver.dwBuildNumber & 0xFFFF;

        wsprintf(mmb, _T("%u.%u.%u"), majorVersion, minorVersion, buildNumber);

        LogPrintf(_T("Operating system: %s\r\n"), mmb);
    }
    else
        LogPrintf(_T("%s"), "Operating system unknown\r\n");
}

//
// PrintCPUInfo
//
// Prints information on the user's CPU(s)
//
static void PrintCPUInfo(void)
{
    SYSTEM_INFO sysinfo;

    GetSystemInfo(&sysinfo);

    LogPrintf(_T("%d processor%s, type %d.\r\n"), sysinfo.dwNumberOfProcessors,
              sysinfo.dwNumberOfProcessors > 1 ? _T("s") : _T(""), sysinfo.dwProcessorType);
}

//
// PrintMemInfo
//
// Prints memory usage information.
//
static void PrintMemInfo(void)
{
    MEMORYSTATUSEX meminfo;

    meminfo.dwLength = sizeof(meminfo);

    GlobalMemoryStatusEx(&meminfo);

    LogPrintf(_T("%d%% memory in use.\r\n"), meminfo.dwMemoryLoad);
    LogPrintf(_T("%I64u MB physical memory.\r\n"), RoundMem(meminfo.ullTotalPhys));
    LogPrintf(_T("%I64u MB physical memory free.\r\n"), RoundMem(meminfo.ullAvailPhys));
    LogPrintf(_T("%I64u MB page file.\r\n"), RoundMem(meminfo.ullTotalPageFile));
    LogPrintf(_T("%I64u MB paging file free.\r\n"), RoundMem(meminfo.ullAvailPageFile));
    LogPrintf(_T("%I64u MB user address space.\r\n"), RoundMem(meminfo.ullTotalVirtual));
    LogPrintf(_T("%I64u MB user address space free.\r\n"), RoundMem(meminfo.ullAvailVirtual));
}

//
// PrintSegVInfo
//
// For access violations, this prints additional information.
//
static void PrintSegVInfo(void)
{
    TCHAR        msg[1024];
    const TCHAR *readOrWrite = _T("read");

    if(exceptionRecord->ExceptionInformation[0])
        readOrWrite = _T("written");

#ifdef _M_IX86
    wsprintf(msg, _T("Access violation at %08x. The memory could not be %s.\r\n"),
             exceptionRecord->ExceptionInformation[1], readOrWrite);
#else
    wsprintf(msg, _T("Access violation at %016I64x. The memory could not be %s.\r\n"),
             exceptionRecord->ExceptionInformation[1], readOrWrite);
#endif

    LogPrintf(_T("%s"), msg);
}

//
// PrintRegInfo
//
// Prints registers.
//
static void PrintRegInfo(void)
{
    LogPrintf(_T("\r\nContext:\r\n"));
#ifdef _M_IX86
    LogPrintf(_T("EDI:    0x%08x  ESI: 0x%08x  EAX:   0x%08x\r\n"), contextRecord->Edi, contextRecord->Esi,
              contextRecord->Eax);
    LogPrintf(_T("EBX:    0x%08x  ECX: 0x%08x  EDX:   0x%08x\r\n"), contextRecord->Ebx, contextRecord->Ecx,
              contextRecord->Edx);
    LogPrintf(_T("EIP:    0x%08x  EBP: 0x%08x  SegCs: 0x%08x\r\n"), contextRecord->Eip, contextRecord->Ebp,
              contextRecord->SegCs);
    LogPrintf(_T("EFlags: 0x%08x  ESP: 0x%08x  SegSs: 0x%08x\r\n"), contextRecord->EFlags, contextRecord->Esp,
              contextRecord->SegSs);
#else
    LogPrintf(_T("SegCs:        0x%08x SegSs: 0x%08x\r\n"), contextRecord->SegCs, contextRecord->SegSs);
    LogPrintf(_T("ContextFlags: 0x%08x MxCsr: 0x%08x EFlags: 0x%08x\r\n"), contextRecord->ContextFlags,
              contextRecord->MxCsr, contextRecord->EFlags);
    LogPrintf(_T("Rax: 0x%016I64x Rcx: 0x%016I64x Rdx: 0x%016I64x\r\n"), contextRecord->Rax, contextRecord->Rcx,
              contextRecord->Rdx);
    LogPrintf(_T("Rbx: 0x%016I64x Rsp: 0x%016I64x Rbp: 0x%016I64x\r\n"), contextRecord->Rbx, contextRecord->Rsp,
              contextRecord->Rbp);
    LogPrintf(_T("Rsi: 0x%016I64x Rdi: 0x%016I64x R8:  0x%016I64x\r\n"), contextRecord->Rsi, contextRecord->Rdi,
              contextRecord->R8);
    LogPrintf(_T("R9:  0x%016I64x R10: 0x%016I64x R11: 0x%016I64x\r\n"), contextRecord->R9, contextRecord->R10,
              contextRecord->R11);
    LogPrintf(_T("R12: 0x%016I64x R13: 0x%016I64x R14: 0x%016I64x\r\n"), contextRecord->R12, contextRecord->R13,
              contextRecord->R14);
    LogPrintf(_T("R15: 0x%016I64x Rip: 0x%016I64x\r\n"), contextRecord->R15, contextRecord->Rip);
    LogPrintf(_T("Dr0: 0x%016I64x Dr1: 0x%016I64x Dr2: 0x%016I64x\r\n"), contextRecord->Dr0, contextRecord->Dr1,
              contextRecord->Dr2);
    LogPrintf(_T("Dr3: 0x%016I64x Dr6: 0x%016I64x Dr7: 0x%016I64x\r\n"), contextRecord->Dr3, contextRecord->Dr6,
              contextRecord->Dr7);
    LogPrintf(_T("DebugControl:         0x%016I64x LastBranchToRip:    0x%016I64x\r\n"), contextRecord->DebugControl,
              contextRecord->LastBranchToRip);
    LogPrintf(_T("LastBranchFromRip:    0x%016I64x LastExceptionToRip: 0x%016I64x\r\n"),
              contextRecord->LastBranchFromRip, contextRecord->LastExceptionToRip);
    LogPrintf(_T("LastExceptionFromRip: 0x%016I64x\r\n"), contextRecord->LastExceptionFromRip);
#endif
}

//
// PrintCS
//
// Prints out some of the memory at the instruction pointer.
//
static void PrintCS(void)
{
#ifdef _M_IX86
    BYTE *ipaddr = (BYTE *)contextRecord->Eip;
#else
    BYTE *ipaddr = (BYTE *)contextRecord->Rip;
#endif
    int i;

    LogPrintf(_T("\r\nBytes at CS:IP:\r\n"));

    for(i = 0; i < CODETOPRINT; ++i)
    {
        // must check for exception, in case of invalid instruction pointer
        __try
        {
            LogPrintf(_T("%02x "), ipaddr[i]);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            LogPrintf(_T("?? "));
        }
    }
}

// Note: everything inside here is x86-specific, unfortunately.
#ifdef _M_IX86
//
// PrintStack
//
// Prints out the contents of the stack.
//
static void PrintStack(void)
{
    DWORD *stackptr = (DWORD *)contextRecord->Esp;
    DWORD *stacktop;
    DWORD *stackstart;
    int    cnt        = 0;
    int    numPrinted = 0;

    LogPrintf(_T("\r\n\r\nStack:\r\n"));

    __try
    {
        __asm
        {
            // Load the top address of the stack from the thread information block.
         mov   eax, fs: [4]
         mov   stacktop, eax
        }

        if(stacktop > stackptr + STACKTOPRINT) stacktop = stackptr + STACKTOPRINT;

        stackstart = stackptr;

        while(stackptr + 1 <= stacktop)
        {
            if((cnt % 4) == 0)
            {
                stackstart = stackptr;
                numPrinted = 0;
                LogPrintf(_T("0x%08x: "), stackptr);
            }

            if((++cnt % 4) == 0 || stackptr + 2 > stacktop)
            {
                int i, n;

                LogPrintf(_T("%08x "), *stackptr);
                ++numPrinted;

                n = numPrinted;

                while(n < 4)
                {
                    LogPrintf(_T("         "));
                    ++n;
                }

                for(i = 0; i < numPrinted; ++i)
                {
                    int   j;
                    DWORD stackint = *stackstart;

                    for(j = 0; j < 4; ++j)
                    {
                        char c = (char)(stackint & 0xFF);
                        if(c < 0x20 || c > 0x7E)
                            c = '.';
                        LogPrintf(_T("%c"), c);
                        stackint = stackint >> 8;
                    }
                    ++stackstart;
                }

                LogPrintf(_T("\r\n"));
            }
            else
            {
                LogPrintf(_T("%08x "), *stackptr);
                ++numPrinted;
            }
            ++stackptr;
        }

        LogPrintf(_T("\r\n"));
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LogPrintf(_T("Could not access stack.\r\n"));
    }
}
#else
struct _TEB
{
    NT_TIB NtTib;
};

//
// PrintStack
//
// Prints out the contents of the stack.
//
static void PrintStack(void)
{
    DWORD64 *stackptr = (DWORD64 *)contextRecord->Rsp;
    DWORD64 *stacktop;
    DWORD64 *stackstart;
    int      cnt        = 0;
    int      numPrinted = 0;

    LogPrintf(_T("\r\nStack:\r\n"));

    __try
    {
        stacktop = (DWORD64 *)NtCurrentTeb()->NtTib.StackBase;

        if(stacktop > stackptr + STACKTOPRINT)
            stacktop = stackptr + STACKTOPRINT;

        stackstart = stackptr;

        while(stackptr + 1 <= stacktop)
        {
            if((cnt % 4) == 0)
            {
                stackstart = stackptr;
                numPrinted = 0;
                LogPrintf(_T("0x%016I64x: "), stackptr);
            }

            if((++cnt % 4) == 0 || stackptr + 2 > stacktop)
            {
                int i, n;

                LogPrintf(_T("%016I64x "), *stackptr);
                ++numPrinted;

                n = numPrinted;

                while(n < 4)
                {
                    LogPrintf(_T("         "));
                    ++n;
                }

                for(i = 0; i < numPrinted; ++i)
                {
                    int     j;
                    DWORD64 stackint = *stackstart;

                    for(j = 0; j < 8; ++j)
                    {
                        char c = (char)(stackint & 0xFF);
                        if(c < 0x20 || c > 0x7E)
                            c = '.';
                        LogPrintf(_T("%c"), c);
                        stackint = stackint >> 8;
                    }
                    ++stackstart;
                }

                LogPrintf(_T("\r\n"));
            }
            else
            {
                LogPrintf(_T("%016I64x "), *stackptr);
                ++numPrinted;
            }
            ++stackptr;
        }

        LogPrintf(_T("\r\n"));
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LogPrintf(_T("Could not access stack.\r\n"));
    }
}
#endif // _M_IX86

//
// PrintBacktrace
//
// Prints out a clean backtrace of the stack. Only available for x64.
//
static void PrintBacktrace(void)
{
#ifdef _M_AMD64
    __try
    {
        CONTEXT context;
        memcpy(&context, contextRecord, sizeof(context));

        UNWIND_HISTORY_TABLE unwindHist;
        memset(&unwindHist, 0, sizeof(unwindHist));

        LogPrintf(_T("\r\n\r\nBacktrace:\r\n"));

        while(context.Rip != 0)
        {
            ULONG64                       imgBase = 0;
            PRUNTIME_FUNCTION             func    = RtlLookupFunctionEntry(context.Rip, &imgBase, &unwindHist);
            KNONVOLATILE_CONTEXT_POINTERS nvContext;
            memset(&nvContext, 0, sizeof(nvContext));

            void   *handlerData      = 0;
            ULONG64 establisherFrame = 0;
            if(func == 0)
            {
                // leaf function
                LogPrintf(_T("%016I64x\r\n"), context.Rip);
                context.Rip  = static_cast<ULONG64>(*reinterpret_cast<const ULONG64 *>(context.Rsp));
                context.Rsp += 8;
            }
            else
            {
                // Non-leaf function, unwind
                LogPrintf(_T("%016I64x\r\n"), context.Rip);
                RtlVirtualUnwind(0, imgBase, context.Rip, func, &context, &handlerData, &establisherFrame, &nvContext);
            }
        }

        LogPrintf(_T("\r\n"));
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LogPrintf(_T("Could not access stack.\r\n"));
    }
#endif
}

//
// WriteMinidump
//
// Writes out a minidump file
//

typedef BOOL(WINAPI *MINIDUMP_WRITE_DUMP)(IN HANDLE hProcess, IN DWORD ProcessId, IN HANDLE hFile,
                                          IN MINIDUMP_TYPE                                         DumpType,
                                          IN CONST PMINIDUMP_EXCEPTION_INFORMATION                 ExceptionParam,
                                          OPTIONAL IN PMINIDUMP_USER_STREAM_INFORMATION            UserStreamParam,
                                          OPTIONAL IN PMINIDUMP_CALLBACK_INFORMATION CallbackParam OPTIONAL);

void WriteMinidump(_EXCEPTION_POINTERS *pException)
{
    HMODULE hDbgHelp = LoadLibrary(_T("DBGHELP.DLL"));
    if(!hDbgHelp)
    {
        return;
    }
    MINIDUMP_WRITE_DUMP MiniDumpWriteDump_ = (MINIDUMP_WRITE_DUMP)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");

    if(MiniDumpWriteDump_)
    {
        MINIDUMP_EXCEPTION_INFORMATION M;
        HANDLE                         hDump_File;
        TCHAR                          Dump_Path[MAX_PATH];

        M.ThreadId          = GetCurrentThreadId();
        M.ExceptionPointers = pException;
        M.ClientPointers    = 0;

        GetModuleFileName(NULL, Dump_Path, sizeof(Dump_Path));
        lstrcpy(Dump_Path + lstrlen(Dump_Path) - 3, _T("dmp"));

        hDump_File = CreateFile(Dump_Path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        MiniDumpWriteDump_(GetCurrentProcess(), GetCurrentProcessId(), hDump_File, MiniDumpNormal,
                           (pException) ? &M : NULL, NULL, NULL);

        CloseHandle(hDump_File);
    }
}

//
// LaunchCrashApp
//
// Runs the crash reporter application.
//
static int LaunchCrashApp(void)
{
    static STARTUPINFOW        si;
    static PROCESS_INFORMATION pi;
    static wchar_t             cmdline[MAX_PATH * 2];

    // Replace the filename with our crash report exe file name
    wcscpy_s(fileName, charcount(moduleName) - (fileName - moduleName), L"eecrashreport.exe");
    wcscpy_s(cmdline, moduleFileName);
    wcscat_s(cmdline, L" \""); // surround app name with quotes

    ZeroMemory(moduleFileName, sizeof(moduleFileName));

    GetModuleFileNameW(nullptr, moduleFileName, charcount(moduleFileName) - 2);

    wcscat_s(cmdline, ExtractFileName(moduleFileName));
    wcscat_s(cmdline, L"\"");

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;

    return CreateProcessW(nullptr, cmdline, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi) ? 1 : 0;
}

//=============================================================================
//
// Main Routine
//

//
// Initialize the exception handler
//
void I_W32InitExceptionHandler(void)
{
    RtlGetVersion =
        reinterpret_cast<decltype(RtlGetVersion)>(GetProcAddress(GetModuleHandleW(L"ntdll"), "RtlGetVersion"));
}

//
// I_W32ExceptionHandler
//
// Main routine for handling Win32 exceptions.
//
int __cdecl I_W32ExceptionHandler(PEXCEPTION_POINTERS ep)
{
    static thread_local int exceptionCaught = 0;

    // if this happens, it means the exception handler crashed. Uh-oh!
    if(exceptionCaught)
        return EXCEPTION_CONTINUE_SEARCH;

    exceptionCaught = 1;

    // Only handle the first overall exception
    bool handleException = false;
    if(!exceptionHandled.compare_exchange_strong(handleException, true))
        return EXCEPTION_EXECUTE_HANDLER;

    // set exception and context pointers
    exceptionRecord = ep->ExceptionRecord;
    contextRecord   = ep->ContextRecord;

    // get module path and name information
    GetModuleName();

    // open exception log
    if(!OpenLogFile())
        return EXCEPTION_CONTINUE_SEARCH;

    PrintHeader();   // output header
    PrintTime();     // time of exception
    PrintUserInfo(); // print user information
    PrintOSInfo();   // print operating system information
    PrintCPUInfo();  // print processor information
    PrintMemInfo();  // print memory usage

    // print additional info for access violations
    if(exceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION && exceptionRecord->NumberParameters >= 2)
        PrintSegVInfo();

    // This won't be terribly useful on non-x86 as-is.
    // That is assuming it works at all, of course.
    PrintRegInfo();   // print CPU registers
    PrintCS();        // print code segment at EIP
    PrintBacktrace(); // print clean backtrace (x64 only)
    PrintStack();     // print stack dump

    LogPrintf(_T("\r\n===== [end of %s] =====\r\n"), _T("CRASHLOG.TXT"));
    LogFlush(logFile);
    CloseHandle(logFile);

    WriteMinidump(ep);

    // The crash reporter app won't run on a stack overflow.
    // Stupid CreateProcess uses too much stack space.
    if(ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
        return EXCEPTION_EXECUTE_HANDLER;

    if(LaunchCrashApp())
        return EXCEPTION_EXECUTE_HANDLER;
    else
        return EXCEPTION_CONTINUE_SEARCH;

    return EXCEPTION_EXECUTE_HANDLER;
}

#endif // defined(_WIN32)

