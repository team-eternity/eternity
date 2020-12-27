// eecrashreport.h : main header file for the EECRASHREPORT application
//

#if !defined(AFX_EECRASHREPORT_H__74317D10_8B4F_4AC8_8CAF_7F1ADF1EC8D2__INCLUDED_)
#define AFX_EECRASHREPORT_H__74317D10_8B4F_4AC8_8CAF_7F1ADF1EC8D2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h" // main symbols

/////////////////////////////////////////////////////////////////////////////
// CEECrashReportApp:
// See eecrashreport.cpp for the implementation of this class
//

class CEECrashReportApp : public CWinApp
{
public:
	CEECrashReportApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEECrashReportApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CEECrashReportApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EECRASHREPORT_H__74317D10_8B4F_4AC8_8CAF_7F1ADF1EC8D2__INCLUDED_)
