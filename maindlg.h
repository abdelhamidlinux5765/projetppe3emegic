// maindlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINDLG_H__58A49173_4DDE_4D58_8454_42C63E71D4B7__INCLUDED_)
#define AFX_MAINDLG_H__58A49173_4DDE_4D58_8454_42C63E71D4B7__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ObexFTP.h"

#define UM_UPDATE (WM_USER+0x100)

class CMainDlg : public CDialogImpl<CMainDlg>,
		public CMessageFilter, public CIdleHandler,
    public CObexFTP
{
public:
	enum { IDD = IDD_MAINDLG };

  CMainDlg() : m_nQueryState(0),m_pConnection(NULL)
  {
  }

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();

  // CObexFTP::OnDeviceArrived
  virtual void OnDeviceArrived(IPropertyBag *Bag);


	BEGIN_MSG_MAP(CMainDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDB_QUERY, OnQuery)
		MESSAGE_HANDLER(UM_UPDATE, OnUpdateDevice)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnQuery(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnUpdateDevice(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	void CloseDialog(int nVal);
  void SizeControl(HWND hwCtrl,int xDiff,int yDiff);
  void LogLine(LPCTSTR pszLine);
  CListBox m_wLog;
  CButton  m_wQuery;
  UINT     m_nQueryState;
  CObexFTPConnection *m_pConnection;
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft eMbedded Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINDLG_H__58A49173_4DDE_4D58_8454_42C63E71D4B7__INCLUDED_)
