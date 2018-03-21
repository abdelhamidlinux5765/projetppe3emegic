// maindlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "maindlg.h"
#pragma comment(lib,"aygshell.lib")

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg)
{
	return CWindow::IsDialogMessage(pMsg);
}

BOOL CMainDlg::OnIdle()
{
	return FALSE;
}


void CMainDlg::SizeControl(HWND hwCtrl,int xDiff,int yDiff)
{
  CWindow wChild(hwCtrl);
  CRect   rChild;
  wChild.GetWindowRect(&rChild);
  ScreenToClient(&rChild);
  rChild.right+=xDiff;
  rChild.bottom+=yDiff;
  wChild.MoveWindow(&rChild);
}


void CMainDlg::LogLine(LPCTSTR pszLine)
{
  while (m_wLog.AddString(pszLine)<0)
  {
    m_wLog.DeleteString(0);
  }
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
  CRect rcTemplate; GetClientRect(&rcTemplate); 
	// center the dialog on the screen
	// CenterWindow();
	SHINITDLGINFO	shidi;
	memset(&shidi, 0, sizeof(SHINITDLGINFO));
	shidi.dwMask = SHIDIM_FLAGS;
	shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN;			
	shidi.hDlg = m_hWnd;
	if (!SHInitDialog(&shidi))
	{
		DWORD dwError = GetLastError();
		ATLTRACE(_T("Warning: Making Fullscreen dialog failed during dialog init. Error # %d\n"),dwError);				
	}
  CRect rcFull; GetClientRect(&rcFull); 
  int cxDiff=rcFull.Width()-rcTemplate.Width();
  int cyDiff=rcFull.Height()-rcTemplate.Height();
  // Now resize the Controls
  m_wQuery=GetDlgItem(IDB_QUERY);
  m_wLog=GetDlgItem(IDC_LOG);
  SizeControl(m_wQuery,cxDiff,0);
  SizeControl(m_wLog,cxDiff,cyDiff);



	// set icons
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

  m_nQueryState=0;

	return TRUE;
}

LRESULT CMainDlg::OnQuery(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  if (!m_nQueryState)
  {
    m_wLog.ResetContent();
    if (!Initialize())
    {
      LogLine(_T("? Cannot init OBEX"));
    }
    else if (!StartDeviceEnumeration())
    {
      LogLine(_T("? Cannot enumerate devices"));
    }
    else
    {
      LogLine(_T("Searching for server ..."));
      m_nQueryState=1;
      m_wQuery.SetWindowText(_T("Stop Synchronization"));
    }
  }
  else
  {
    StopDeviceEnumeration();
    LogLine(_T("Search canceled"));
    m_nQueryState=0;
    m_wQuery.SetWindowText(_T("Start Synchronization"));
  }
  return 0;
}


LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  if (m_pConnection)
  {
    m_pConnection=m_pConnection->Disconnect(true);
  }
	// TODO: Add validation code 
	CloseDialog(wID);
	return 0;
}


void CMainDlg::CloseDialog(int nVal)
{
	DestroyWindow();
	::PostQuitMessage(nVal);
}


void CMainDlg::OnDeviceArrived(IPropertyBag *Bag)
{
  CObexDeviceProperties p(Bag);
  CString strName=p.GetName();
  if (m_pConnection)
  {
    LogLine(strName+_T(" arrived but still busy"));
  }
  else if ((m_pConnection=ConnectTo(Bag))==NULL)
  {
    LogLine(strName+_T(" arrived; connect failed!"));
  }
  else
  {
    ::PostMessage(m_hWnd,UM_UPDATE,0,0);
    LogLine(strName+_T(" connected!"));
  }
}

int TimeCompare(FILETIME &a,FILETIME &b)
{
  __int64 ia=(*(ULARGE_INTEGER *)&a).QuadPart;
  __int64 ib=(*(ULARGE_INTEGER *)&b).QuadPart;
  return ib>ia ? -1 : ib==ia;
}

BOOL ReadTimeFromRegistry(LPCWSTR pszKey,FILETIME &ftRead)
{
  BOOL bResult=FALSE; 
  HKEY hProg; DWORD Disp;
  if (ERROR_SUCCESS==RegCreateKeyEx(HKEY_CURRENT_USER,_T("Software\\PocketOBEX"),0,NULL,0,0,NULL,&hProg,&Disp))
  {
    DWORD Type,cbSize=sizeof(FILETIME);
    if (ERROR_SUCCESS==RegQueryValueEx(hProg,pszKey,NULL,&Type,(BYTE *)&ftRead,&cbSize) &&
        Type==REG_BINARY && cbSize==sizeof(FILETIME))
        bResult=TRUE;
    FILETIME ftNow; SYSTEMTIME stNow;
    GetSystemTime(&stNow); SystemTimeToFileTime(&stNow,&ftNow);
    RegSetValueEx(hProg,pszKey,0,REG_BINARY,(BYTE *)&ftNow,sizeof(FILETIME));
    RegCloseKey(hProg);
  }
  return bResult;
}
LRESULT CMainDlg::OnUpdateDevice(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
  ATLASSERT(m_pConnection);
  // we do not need more the one
  StopDeviceEnumeration();
  if (!m_pConnection->SetPath(_T("SampleFolder"),SETPATH_FLAG_DONT_CREATE))
  {
    LogLine(_T("Cannot cd to SampleFolder"));
  }
  else
  {
    FILETIME ftLast;   
    BOOL bServerSeen=ReadTimeFromRegistry(m_pConnection->GetDeviceName(),ftLast);
    CObexListing Dir;
    WIN32_FIND_DATA fd;
    if (!m_pConnection->GetDirectory(Dir) ||
        !Dir.FindFirstFile(&fd,FILE_ATTRIBUTE_NORMAL))
    {
      LogLine(_T("SampleFolder is empty"));
    }
    else do
    {
      SYSTEMTIME st; TCHAR achTime[80],achDate[80];
      FileTimeToSystemTime(&fd.ftLastWriteTime,&st);
      GetTimeFormat(LOCALE_USER_DEFAULT,0,&st,NULL,achTime,80);
      GetDateFormat(LOCALE_USER_DEFAULT,0,&st,NULL,achDate,80);
      CString strFile;
      if (!bServerSeen || TimeCompare(ftLast,fd.ftLastWriteTime)<0)
        strFile=_T("> ");
      else
        strFile=_T("= ");
      strFile=strFile+_T(" ")+achDate;
      strFile=strFile+_T(" ")+achTime+_T(" ");
      strFile+=fd.cFileName;
      LogLine(strFile);
    } while (Dir.FindNextFile(&fd));
  }
  // Done with this connection
  m_pConnection=m_pConnection->Disconnect(true);
  LogLine(_T("Synchronization done"));
  m_nQueryState=0;
  m_wQuery.SetWindowText(_T("Start Synchronization"));
  return 0;
}
