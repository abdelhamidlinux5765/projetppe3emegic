#ifndef _OBEX_FTP_H_
#define _OBEX_FTP_H_

#include "obexutil.h"

class CObexFTPConnection;

//-------------------------------------------------------------------
// Wrapper class for IObex functionality (OBEX-FTP only!)
//-------------------------------------------------------------------
class CObexFTP : public IObexSink
{
public:
  CObexFTP();
  ~CObexFTP();
  // IObexSink::IUnknown
  HRESULT QueryInterface(REFIID iid,void ** ppvObject);
  ULONG   AddRef();
  ULONG   Release();
  // IObexSink::Notify
  HRESULT Notify(OBEX_EVENT Event,IUnknown* pUnk1,IUnknown* /*pUnk2*/);
  // virtual functions for DeviceArrival/Removal/Update
  virtual void OnDeviceArrived(IPropertyBag *Bag);
  virtual void OnDeviceRemoval(CObexFTPConnection *pConn);
  virtual void OnDeviceChange(CObexFTPConnection *pConn,IPropertyBag *Bag);
  // Initialize the OBEX system
  BOOL Initialize();
  // Start/Stop Enumeration
  BOOL StartDeviceEnumeration();
  void StopDeviceEnumeration();
  // Create a standard CObexFTPConnection
  CObexFTPConnection *ConnectTo(IPropertyBag *Prop,LPCWSTR pszPassw=NULL);
  // raw pointer for the IObex
	operator IObex*() const
	{
		return (IObex*)m_obex;
	}
protected:
  // for CObexDTPConnection only
  friend class CObexFTPConnection;
  BOOL Connect(CObexFTPConnection *pConn,IPropertyBag *Prop,IObexDevice **ppOut);
  BOOL Disconnect(CObexFTPConnection *pConn);
protected:
  CObexFTPConnection *FindConnection(const CString &Key);
  void AddConnection(CObexFTPConnection *pConn);
  CComPtr<IObex> m_obex;
  LONG    m_cRef;
  DWORD   m_dwAdvise;
  bool    m_bEnumerating;
  CSimpleList<CObexFTPConnection> m_Connections;
};

//-------------------------------------------------------------------
// Wrapper class for IObexDevice functionality (OBEX-FTP only!)
//-------------------------------------------------------------------
class CObexFTPConnection
{
public:
  CObexFTPConnection(LPCWSTR pszPassw=NULL);
  ~CObexFTPConnection();
  // virtual functions for DeviceRemoval/Update
  virtual void OnDeviceRemoval();
  virtual void OnDeviceChange(IPropertyBag *Bag);
  // create/close a connection
  BOOL Connect(CObexFTP *pObj,IPropertyBag *Bag);
  CObexFTPConnection *Disconnect(bool bAutoDel=false);
  // ------ FTP-Functions ----------------------
  BOOL SetPath(LPCWSTR pszPath,DWORD dwFlags=0);
  BOOL GetFile(LPCWSTR pszName,CStreamToMemory &Mem);
  BOOL PutFile(LPCWSTR pszName,void *Data,UINT Size);
  BOOL GetDirectory(CObexListing &Dir);
  // called e.g. from CObexFTP::Connect
  const CString &GetPassword()
  {
    return m_strPassw;
  }
  // raw pointer for the IObexDevice
	operator IObexDevice*() const
	{
		return (IObexDevice*)m_device;
	}
// Needed for simple List
  CObexFTPConnection *m_pNext;
  BOOL IsKey(LPCWSTR pszKey)
  {
    return 0==wcsicmp(pszKey,m_strKey);
  }
  void SetKey(LPCWSTR pszKey)
  {
    m_strKey=pszKey;
  }
  CString GetDeviceName();
protected:
  BOOL GetFileOrDir(LPCWSTR pszName,const char *mime,CStreamToMemory &Mem);
protected:
  CString m_strKey;
  CString m_strPassw;
  CObexFTP *m_pObj;
  CComPtr<IObexDevice> m_device;
};

#endif
