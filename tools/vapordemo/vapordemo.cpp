// vapordemo.cpp : Defines the entry point for the application.
//
// You can do whatever you want with this. Released to the public domain. - Quasar.

#include "stdafx.h"

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	PROCESS_INFORMATION pi;
	STARTUPINFO si;

	memset(&si, 0, sizeof(si));

	si.cb = sizeof(si);

	CreateProcess("eternity.exe", "eternity.exe -game vapordemo", NULL, NULL, 
		          FALSE, 0, NULL, NULL, &si, &pi);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return 0;
}



