// eecrashreportDlg.h : header file
//

#if !defined(AFX_EECRASHREPORTDLG_H__C420F663_FC04_43BF_8E4E_327E5A149B06__INCLUDED_)
#define AFX_EECRASHREPORTDLG_H__C420F663_FC04_43BF_8E4E_327E5A149B06__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CEECrashReportDlg dialog

class CEECrashReportDlg : public CDialog
{
    // Construction
public:
    CEECrashReportDlg(CWnd *pParent = NULL); // standard constructor

    // Dialog Data
    //{{AFX_DATA(CEECrashReportDlg)
    enum
    {
        IDD = IDD_EECRASHREPORT_DIALOG
    };
    CEdit m_ErrorEditBox;
    //}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CEECrashReportDlg)
protected:
    virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support
                                                     //}}AFX_VIRTUAL

    // Implementation
protected:
    HICON  m_hIcon;
    size_t filelen;

    // haleyjd: custom methods
    void LoadErrorFile();
    void LaunchBrowser();

    // Generated message map functions
    //{{AFX_MSG(CEECrashReportDlg)
    virtual BOOL    OnInitDialog();
    afx_msg void    OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void    OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    virtual void    OnOK();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EECRASHREPORTDLG_H__C420F663_FC04_43BF_8E4E_327E5A149B06__INCLUDED_)
