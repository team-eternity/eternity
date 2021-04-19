// eecrashreportDlg.cpp : implementation file
//

#include "stdafx.h"
#include "eecrashreport.h"
#include "eecrashreportDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEECrashReportDlg dialog

CEECrashReportDlg::CEECrashReportDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CEECrashReportDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CEECrashReportDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CEECrashReportDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEECrashReportDlg)
	DDX_Control(pDX, IDC_EDIT1, m_ErrorEditBox);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CEECrashReportDlg, CDialog)
	//{{AFX_MSG_MAP(CEECrashReportDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEECrashReportDlg message handlers

BOOL CEECrashReportDlg::OnInitDialog()
{
   CDialog::OnInitDialog();
   
   // Add "About..." menu item to system menu.
   
   // IDM_ABOUTBOX must be in the system command range.
   ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
   ASSERT(IDM_ABOUTBOX < 0xF000);
   
   CMenu* pSysMenu = GetSystemMenu(FALSE);
   if (pSysMenu != NULL)
   {
      CString strAboutMenu;
      strAboutMenu.LoadString(IDS_ABOUTBOX);
      if (!strAboutMenu.IsEmpty())
      {
         pSysMenu->AppendMenu(MF_SEPARATOR);
         pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
      }
   }
   
   // Set the icon for this dialog.  The framework does this automatically
   //  when the application's main window is not a dialog
   SetIcon(m_hIcon, TRUE);			// Set big icon
   SetIcon(m_hIcon, FALSE);		// Set small icon
   
   // haleyjd: extra initialization
   HICON errorIcon = AfxGetApp()->LoadStandardIcon(MAKEINTRESOURCE(IDI_ERROR));
   ((CStatic *)GetDlgItem(IDC_STATIC_ICON))->SetIcon(errorIcon);
   
   // load text from crashlog.txt file
   LoadErrorFile();
   
   return TRUE;  // return TRUE  unless you set the focus to a control
}

void CEECrashReportDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CEECrashReportDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CEECrashReportDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CEECrashReportDlg::OnOK() 
{
   // copy data in message box to the clipboard
   m_ErrorEditBox.SendMessage(EM_SETSEL, 0, filelen);
   m_ErrorEditBox.SendMessage(WM_COPY,   0, 0);
   m_ErrorEditBox.SendMessage(EM_SETSEL, 0, 0);
   
   LaunchBrowser();
   
   //CDialog::OnOK();
}

//
// CEECrashReportDlg::LoadErrorFile
//
// haleyjd: this function loads crashlog.txt from the application's directory.
//
void CEECrashReportDlg::LoadErrorFile()
{
   char *str;
   FILE *f;
   char moduleName[2*MAX_PATH];
   char *filename;

   memset(moduleName, 0, sizeof(moduleName));

   GetModuleFileName(NULL, moduleName, MAX_PATH);

   filename = strrchr(moduleName, '\\');

   if(filename)
   {
      char *tmp = filename;

      while(*filename != '\0')
         *filename++ = '\0';

      strcat(moduleName, "\\crashlog.txt");
   }      
   else
   {
      memset(moduleName, 0, sizeof(moduleName));
      strcpy(moduleName, "crashlog.txt");
   }

   if(!(f = fopen(moduleName, "rb")))
   {
      AfxMessageBox("crashlog.txt not found.", MB_OK | MB_ICONEXCLAMATION);
      return;
   }

   // seek to end of file and get offset
   fseek(f, 0, SEEK_END);
   filelen = (size_t)ftell(f);
   rewind(f);

   if(filelen == 0)
   {
      AfxMessageBox("crashlog.txt is empty.", MB_OK | MB_ICONEXCLAMATION);
      fclose(f);
      return;
   }

   // read file
   str = new char [filelen + 1];
   memset(str, 0, filelen + 1);
   fread(str, 1, filelen, f);

   // done with file
   fclose(f);

   // set string to message box
   m_ErrorEditBox.SetWindowText(str);

   // done with string (I hope)
   delete [] str;
}

//
// CEECrashReportDlg::LaunchBrowser
//
// Opens a new browser tab or window in the user's default web browser
// pointing to the Eternity forums.
//
void CEECrashReportDlg::LaunchBrowser()
{
   ShellExecute(NULL, "open", 
                "https://www.doomworld.com/forum/25-eternity/?do=add", 
                NULL, NULL, SW_SHOWNORMAL);
}
