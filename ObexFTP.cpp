#include "stdafx.h"
#include "obexFTP.h"
#include <ObexStrings.h>
#include <bt_sdp.h>

#pragma comment(lib,"comsupp.lib")
#pragma comment(lib,"ccrtrtti.lib")

//-----------------------------------------------------------------
// CObexDeviceProperties: ctor(1) with an IPropertyBag *
// useful for OnDeviceXXX functions
//-----------------------------------------------------------------
CObexDeviceProperties::CObexDeviceProperties(IPropertyBag *Prop) : m_Bag(Prop)
{
}
//-----------------------------------------------------------------
// CObexDeviceProperties: ctor(2) with an IObexDevice *
// first query the device for the Properties
//-----------------------------------------------------------------
CObexDeviceProperties::CObexDeviceProperties(IObexDevice *Dev)
{
  IPropertyBag *Bag=NULL;
  if (Dev && SUCCEEDED(Dev->EnumProperties(__uuidof(IPropertyBag),(void **)&Bag)))
  {
    m_Bag=Bag;
  }
}
//-----------------------------------------------------------------
// CObexDeviceProperties: ctor(3) with an IUnknown *
// used internally in IObexSink::Notify
//-----------------------------------------------------------------
CObexDeviceProperties::CObexDeviceProperties(IUnknown *Unk)
{
  CComQIPtr<IPropertyBag> Bag(Unk);
  if (Bag)
  {
    m_Bag=Bag.Detach();
  }
}

//-----------------------------------------------------------------
// CObexDeviceProperties: dtor - noop
//-----------------------------------------------------------------
CObexDeviceProperties::~CObexDeviceProperties()
{
}

//-----------------------------------------------------------------
// CObexDeviceProperties: GetProperty
// Basic call to read any property
//-----------------------------------------------------------------
HRESULT CObexDeviceProperties::GetProperty(LPCOLESTR PropName,VARIANT &Value)
{
  if (m_Bag)
  {
    return m_Bag->Read(PropName,&Value,NULL);
  }
  else
  {
    return E_FAIL;
  }
}
//-----------------------------------------------------------------
// CObexDeviceProperties: GetProperty
// return the string value of a  property
//-----------------------------------------------------------------
CString CObexDeviceProperties::GetProperty(LPCOLESTR PropName)
{
  CComVariant varValue;;
  CString strValue;
  if (SUCCEEDED(GetProperty(PropName,varValue)))
  {
    if (SUCCEEDED(varValue.ChangeType(VT_BSTR)))
    {
      strValue=varValue.bstrVal;
    }
  }
  return strValue;
}

CString CObexDeviceProperties::GetName()     // NAME of the Device
{
  return GetProperty(c_szDevicePropName);
}
CString CObexDeviceProperties::GetAddress() // ADDRESS of the device
{
  return GetProperty(c_szDevicePropAddress);
}
CString CObexDeviceProperties::GetTransport() // TRANSPORT of the device
{
  return GetProperty(c_szDevicePropTransport);
}
CString CObexDeviceProperties::GetServices()  // SERVICEs of the device
{
  return GetProperty(c_szDeviceServiceUUID);
}
bool CObexDeviceProperties::CanFTP()          // OBEX-FTP supported?
{
  CString strServ=GetServices();
  if (!strServ.IsEmpty())
  {
    LPCWSTR pszServ=strServ;
    for ( ; (pszServ=wcschr(pszServ,L'{'))!=NULL; ++pszServ)
    {
      UINT uuid16=wcstoul(pszServ+1,NULL,16);
      if (uuid16==OBEXFileTransferServiceClassID_UUID16) 
        return true;
    }
  }
  return false;
}


CStreamToMemory::CStreamToMemory(UINT Initial/*=0*/,UINT Add/*=4096*/) 
  : m_Buffer(NULL),m_PageSize(Add),m_Allocated(0),m_Filled(0)
{
  if (Initial)
  {
     m_Buffer=(BYTE *)malloc(Initial);
     if (m_Buffer) m_Allocated=Initial;
  }
}
CStreamToMemory::~CStreamToMemory()
{
  Clear();
}
BOOL CStreamToMemory::Read(IStream *pStream)
{
  if (pStream==NULL)
  {
    return FALSE;
  }
  ULONG cbJustRead,cbSum=0;
  BYTE aTempBuffer[1024];
  while (SUCCEEDED(pStream->Read(aTempBuffer,1024,&cbJustRead)))
  {
    if (m_Filled+cbJustRead>=m_Allocated)
    {
      UINT newAlloc=m_Allocated;
      while (m_Filled+cbJustRead>newAlloc)
        newAlloc+=m_PageSize;
      if (m_Allocated==0) m_Buffer=(BYTE *)malloc(m_Allocated=newAlloc);
      else                m_Buffer=(BYTE *)realloc(m_Buffer,newAlloc);
      if (m_Buffer==NULL) return FALSE;
    }
    memcpy(m_Buffer+m_Filled,aTempBuffer,cbJustRead);
    // Make sure there is a additional zero byte for ascii files
    m_Buffer[m_Filled+=cbJustRead]=0;
    cbSum+=cbJustRead;
  }
  return cbSum!=0;
}

UINT CStreamToMemory::Size()
{
  return m_Filled;
}
BYTE *CStreamToMemory::GetData()
{
  return m_Buffer;
}
void CStreamToMemory::Clear()
{
  free(m_Buffer); m_Buffer=NULL;
  m_Filled=m_Allocated=0;
}


CObexListing::CObexListing(UINT Initial/*=0*/,UINT Add/*=4096*/) 
 : CStreamToMemory(Initial,Add),m_pCur(NULL)
{
}
CObexListing::~CObexListing()
{
}
// Format of the obex-filelist date properties
// yyyymmddThhmmssZ
void CObexListing::GetDateProp(const char *p,FILETIME &ft)
{
  SYSTEMTIME st;
	st.wYear = 1000 * (p[0] - '0') + 100 * (p[1] - '0') + 10 * (p[2] - '0') + (p[3] - '0');
	st.wMonth = 10 * (p[4] - '0') + (p[5] - '0');
	st.wDay = 10 * (p[6] - '0') + (p[7] - '0');
	st.wHour = 10 * (p[9] - '0') + (p[10] - '0');
	st.wMinute = 10 * (p[11] - '0') + (p[12] - '0');
	st.wSecond = 10 * (p[13] - '0') + (p[14] - '0');
	SystemTimeToFileTime(&st, &ft);
}
// if the text text at current pointer m_pCur matches <t>, advance m_pCur and return true
// otherwise do not touch m_pCur and return false
bool CObexListing::AtText(const char *t)
{
  int tl=strlen(t);
  if (0==_strnicmp(m_pCur,t,tl))
  {
    m_pCur+=tl;
    return true;
  }
  return false;
}
// Skip white space
char *CObexListing::Skip(const char *t)
{
  while (isspace(*t)) ++t;
  return (char *)t;
}
// if the text at the current pointer m_pCur matches <t>, then analyse a
// property as: name = "value"
// if the string is properly formatted then
// point <ppAt> to the fisrst chat of value
// replace the terminating " with NUL 
// return true
// Caution: you have to call NextProp to revert the changes!
bool CObexListing::IsProp(const char *t,char **pAt)
{
  char *save=m_pCur;
  if (AtText(t))
  {
    char *p=m_pCur; m_pCur=save;
    if (isspace(*p)) p=Skip(t);
    if (*p++!='=') return false;
    p=Skip(p);
    if (*p++!='\"') return false;
    *pAt=p;
    while (*p && *p!='\"') ++p;
    if (*p)
    {
      *(m_pCur=p)=0;
      return true;
    }
  }
  return false;
}
// revert the changes made by IsProp and advance
// m_pCur to point to the next property
void CObexListing::NextProp()
{
  *m_pCur++='\"';
  m_pCur=Skip(m_pCur);
}
// Skip an unknown property
void CObexListing::SkipProp()
{
  char *p=strchr(m_pCur,'\"');
  if (p)
    p=strchr(p+1,'\"');
  if (p)
    m_pCur=Skip(p+1);
  else
    m_pCur+=strlen(m_pCur);
}


// assume a well formed obex folder-list
BOOL CObexListing::FindFirstFile(WIN32_FIND_DATA *FindFileData,UINT Mask)
{
  m_Mask=Mask&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_NORMAL);
  if (Size()==0 || (m_pCur=(char *)GetData())==NULL)
    return FALSE;
  m_pCur=strstr(m_pCur,"<folder-listing");
  if (m_pCur==NULL)
    return FALSE;
  else
  {
    ++m_pCur;
    if (m_Mask&FILE_ATTRIBUTE_DIRECTORY)
    {
      // Is there are parent folder available
      char *parent=strstr(m_pCur,"<parent-folder");
      if (parent)
      {
        memset(FindFileData,0,sizeof(WIN32_FIND_DATA));
        wcscpy(FindFileData->cFileName,L"..");
        FindFileData->dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY;
        return true;
      }
    }
    return FindNextFile(FindFileData);
  }
}
BOOL CObexListing::FindNextFile(WIN32_FIND_DATA *FindFileData)
{
  while (1)
  {
    if (m_pCur==NULL || (m_pCur=strstr(m_pCur,"<f"))==NULL)
      return FALSE;
    memset(FindFileData,0,sizeof(WIN32_FIND_DATA));
    if (AtText("<folder "))
      FindFileData->dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY;
    else if (AtText("<file "))
      FindFileData->dwFileAttributes=FILE_ATTRIBUTE_NORMAL;
    else
    {
      m_pCur=NULL;
      return FALSE;
    }
    char *PropAt=strstr(m_pCur,"/>");
    if (PropAt==NULL)
    {
      m_pCur=NULL;
      return FALSE;
    }
    if ((FindFileData->dwFileAttributes&m_Mask)==0)
    {
      // not interested in this kind of oject: file/folder
      m_pCur=Skip(PropAt+2);
    }
    else
    {
       // interpret the properties
      *PropAt=0;
      UINT dateFlags=0;
      while (*m_pCur)
      {
        if (IsProp("name",&PropAt))
        {
    	    MultiByteToWideChar (CP_ACP, 0, PropAt, -1, FindFileData->cFileName, MAX_PATH);
          NextProp();
        }
        else if (IsProp("size",&PropAt))
        {
          FindFileData->nFileSizeLow=atoi(PropAt);
          NextProp();
        }
        else if (IsProp("created",&PropAt))
        {
          GetDateProp(PropAt,FindFileData->ftCreationTime);
          dateFlags|=1;
          NextProp();
        }
        else if (IsProp("modified",&PropAt))
        {
          GetDateProp(PropAt,FindFileData->ftLastWriteTime);
          dateFlags|=2;
          NextProp();
        }
        else if (IsProp("accessed",&PropAt))
        {
          GetDateProp(PropAt,FindFileData->ftLastAccessTime);
          dateFlags|=4;
          NextProp();
        }
        else
        {
          SkipProp();
        }
      }
      *m_pCur++='/'; m_pCur++;
      if (dateFlags && dateFlags!=7)
      {
        if ((dateFlags&1)==0)
        {
          if (dateFlags&2) FindFileData->ftCreationTime=FindFileData->ftLastWriteTime;
          else             FindFileData->ftCreationTime=FindFileData->ftLastAccessTime; 
        }
        if ((dateFlags&2)==0)
        {
          if (dateFlags&1) FindFileData->ftLastWriteTime=FindFileData->ftCreationTime;
          else             FindFileData->ftLastWriteTime=FindFileData->ftLastAccessTime; 
        }
        if ((dateFlags&4)==0)
        {
          if (dateFlags&2) FindFileData->ftLastAccessTime=FindFileData->ftLastWriteTime;
          else             FindFileData->ftLastAccessTime=FindFileData->ftCreationTime; 
        }
      }
      return TRUE;
    }
  }
}

BOOL CObexListing::FindClose()
{
  Clear();
  return TRUE;
}

//-------------------------------------------------------------------
// CObexFTP ctor - call Initialize after construction !
//-------------------------------------------------------------------
CObexFTP::CObexFTP() : m_cRef(1),m_bEnumerating(false)
{
}
//-------------------------------------------------------------------
// CObexFTP dtor - stop enumeration, relaese connection point, and
//                 shutdown IObex
//-------------------------------------------------------------------
CObexFTP::~CObexFTP()
{
  // close all active connections
  CObexFTPConnection *c;
  while ((c=m_Connections.m_pHead)!=NULL)
  {
    c->Disconnect();
  }
  if (m_obex)
  {
    StopDeviceEnumeration();
    m_obex->Shutdown();
    m_obex=NULL;
  }
}

//-------------------------------------------------------------------
// CObexFTP::Initialize  - call after ctor to init the obex system
//-------------------------------------------------------------------
BOOL CObexFTP::Initialize()
{
  // already initialzed ?
  if (m_obex)
  {
    return TRUE;
  }
  else
  {
    // CoCreate an instance of IObex
    m_obex.CoCreateInstance(__uuidof(Obex));
    if (m_obex)
    {
      // Call IObex::Initialize
      if (SUCCEEDED(m_obex->Initialize()))
      {
        // Advise the IObexSink connection point
        LPUNKNOWN pUnkThis;
        QueryInterface(IID_IUnknown,(void **)&pUnkThis);
        m_dwAdvise=0;
        if (SUCCEEDED(AtlAdvise(m_obex,pUnkThis,IID_IObexSink,&m_dwAdvise)))
        {
          return TRUE;
        }
        else
        {
          m_obex=NULL;
          ATLTRACE(_T("Cannot Advise!\n"));
        }
      }
      else
      {
        m_obex=NULL;
        ATLTRACE(_T("Cannot IObex::Initialize!\n"));
      }
    }
#ifdef _DEBUG
    else
    {
      ATLTRACE(_T("Cannot CoCreate IObex!\n"));
    }
#endif
  }
  return FALSE;
}
 
//-------------------------------------------------------------------
// CObexFTP::StartDeviceEnum - start asynchronous events via IObexSink
//-------------------------------------------------------------------
BOOL CObexFTP::StartDeviceEnumeration()
{
  if (!m_bEnumerating)
  {
    if (m_obex)
    {
      if (SUCCEEDED(m_obex->StartDeviceEnum()))
        m_bEnumerating=true;
    }
  }
  return m_bEnumerating;
}
//-------------------------------------------------------------------
// CObexFTP::StopDeviceEnum - stop asynchronous events via IObexSink
//-------------------------------------------------------------------
void CObexFTP::StopDeviceEnumeration()
{
  if (m_bEnumerating)
  {
    ATLASSERT((IObex *)m_obex);
    m_obex->StopDeviceEnum();
    m_bEnumerating=false;
  }
}

//-------------------------------------------------------------------
// CObexFTP::IObexSink::IUnknown::Addref - std implementation
//-------------------------------------------------------------------
ULONG CObexFTP::AddRef()
{
  ULONG cr=InterlockedIncrement(&m_cRef);
  return cr;
}
//-------------------------------------------------------------------
// CObexFTP::IObexSink::IUnknown::Release - std implementation
//-------------------------------------------------------------------
ULONG CObexFTP::Release()
{
  ULONG cr=InterlockedDecrement(&m_cRef);
  ATLASSERT(m_cRef);
  return cr;
}
//-------------------------------------------------------------------
// CObexFTP::IObexSink::IUnknown::QueryInterface - std implementation
//-------------------------------------------------------------------
HRESULT CObexFTP::QueryInterface(REFIID iid,void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IObexSink) || 
			InlineIsEqualUnknown(iid))
	{
		if (ppvObject == NULL)
				return E_POINTER;
		*ppvObject = (void*)(IUnknown*)this;
		AddRef();
    return S_OK;
  }
  *ppvObject=NULL;
  return E_NOINTERFACE;
}

//-------------------------------------------------------------------
// CObexFTP::IObexSink::IUnknown::QueryInterface - std implementation
//-------------------------------------------------------------------
HRESULT CObexFTP::Notify(OBEX_EVENT Event,IUnknown* pUnk1,IUnknown* /*pUnk2*/)
{
  // Create a helper object for property access
  CObexDeviceProperties dev(pUnk1);
  ATLASSERT((IPropertyBag *)dev);
  if (dev)
  {
    // build a unique key from Address+ServiceUUID
    CString Key=dev.GetAddress();
    Key+=dev.GetServices();
    // and determine if this device supports OBEX-FTP
    bool IsFTP=dev.CanFTP();
    // if it does - see if its already connected
    if (IsFTP)
    {
      CObexFTPConnection *pConn;
      pConn=FindConnection(Key);
      if (pConn)
      {
        // if yes - call the appropriate handler
        ATLASSERT(Event!=OE_DEVICE_ARRIVAL);
        if (Event==OE_DEVICE_UPDATE)
          OnDeviceChange(pConn,dev);
        else
          OnDeviceRemoval(pConn);
      }
      else if (Event!=OE_DEVICE_DEPARTURE)
      {
        // no this a new one
        OnDeviceArrived(dev);
      }
    }
  }
  return S_OK;
}

//-------------------------------------------------------------------
// CObexFTP::OnDeviceArrived - should be overridden !
//-------------------------------------------------------------------
void CObexFTP::OnDeviceArrived(IPropertyBag *Bag)
{
}

//-------------------------------------------------------------------
// CObexFTP::OnDeviceRemoval - call pConn->OnDeviceRemoval
//-------------------------------------------------------------------
void CObexFTP::OnDeviceRemoval(CObexFTPConnection *pConn)
{
  ATLASSERT(pConn);
  if (pConn)
    pConn->OnDeviceRemoval();
}
//-------------------------------------------------------------------
// CObexFTP::OnDeviceChange - call pConn->OnDeviceChange
//-------------------------------------------------------------------
void CObexFTP::OnDeviceChange(CObexFTPConnection *pConn,IPropertyBag *Prop)
{
  ATLASSERT(pConn);
  if (pConn)
    pConn->OnDeviceChange(Prop);
}

// This is the ServiceUUID of the Obex-FTP in network order
GUID CLSID_FileExchange_NetOrder =	// {F9ec7bc4-953c-11d2-984e-525400dc9e09}
{ 0xc47becf9, 0x3c95, 0xd211, {0x98, 0x4e, 0x52, 0x54, 0x00, 0xdc, 0x9e, 0x09}};

//-------------------------------------------------------------------
// CObexFTP::Connect() - called only from friend CObexFTPConnection
//-------------------------------------------------------------------
BOOL CObexFTP::Connect(CObexFTPConnection *pConn,IPropertyBag *Prop,IObexDevice **ppOut)
{
  // first we must bind the properties to a device
  if (m_obex)
  {
    CComPtr<IObexDevice> Dev;
    if (SUCCEEDED(m_obex->BindToDevice(Prop,&Dev)))
    {
      // Set the password if required
      const CString &Password=pConn->GetPassword();
      if (!Password.IsEmpty())
        Dev->SetPassword(Password);
      // then connect to proper Service
      CComPtr<IHeaderCollection> Coll;
      if (SUCCEEDED(Coll.CoCreateInstance(__uuidof(HeaderCollection))))
      {
         if (SUCCEEDED(Coll->AddTarget(sizeof(CLSID_FileExchange_NetOrder),(BYTE *)&CLSID_FileExchange_NetOrder)))
         {
           // Question: do we need the password here, if it's already set in SetPassword ?
           //           is the dwCap parameter of any interest for any server ?
           if (SUCCEEDED(Dev->Connect(Password,OBEX_DEVICE_CAP_FILE_BROWSE,Coll)))
           {
             // NOTE: may be SUCCEDED is not enough; WINCE samples re-enumerate the HeaderCollection,
             // try to find a CONNECTION_ID-Header and only report success if it is found ?
             *ppOut=Dev.Detach();
             // build the unique key
             CObexDeviceProperties odp(Prop);
             CString Key=odp.GetAddress();
             Key+=odp.GetServices();
             pConn->SetKey(Key);
             // and add to our active list
             AddConnection(pConn);
             return TRUE;
           }
         }
      }
    }
  }
  return FALSE;
}

BOOL CObexFTP::Disconnect(CObexFTPConnection *pConn)
{
  // simply unlink from our list of connections
  return m_Connections.Unlink(pConn)!=NULL;
}

//-------------------------------------------------------------------
// CObexFTP::ConnectTo 
//  use this function if you do not want to use derived classes
//  for CObexFTPConnection
//-------------------------------------------------------------------
CObexFTPConnection *CObexFTP::ConnectTo(IPropertyBag *Prop,LPCWSTR pszPassw/*=NULL*/)
{
  CObexFTPConnection *pConn=new CObexFTPConnection(pszPassw);
  if (!pConn->Connect(this,Prop))
  {
    delete pConn;
    pConn=NULL;
  }
  return pConn;
}

//-------------------------------------------------------------------
// CObexFTP::Connect management: find a connection with unique id
//-------------------------------------------------------------------
CObexFTPConnection *CObexFTP::FindConnection(const CString &Key)
{
  return m_Connections.Find(Key);
}
//-------------------------------------------------------------------
// CObexFTP::Connect management: add a connection to the list
//-------------------------------------------------------------------
void CObexFTP::AddConnection(CObexFTPConnection *pConn)
{
  m_Connections.Push(pConn);
}


//-------------------------------------------------------------------
// CObexFTPConnection ctor
// the optional password is used during connect if the server
// requires this
//-------------------------------------------------------------------
CObexFTPConnection::CObexFTPConnection(LPCWSTR pszPassw/*=NULL*/)
: m_strPassw(pszPassw),m_pNext(NULL),m_pObj(NULL)
{
}
//-------------------------------------------------------------------
// CObexFTPConnection dtor
//-------------------------------------------------------------------
CObexFTPConnection::~CObexFTPConnection()
{
  // if still connected, disconnect
  Disconnect();
}
//-------------------------------------------------------------------
// CObexFTPConnection::OnDeviceRemoval
// should be overwritten to cancel operation an close
//-------------------------------------------------------------------
void CObexFTPConnection::OnDeviceRemoval()
{
}
//-------------------------------------------------------------------
// CObexFTPConnection::OnDeviceChange
// could be overwritten to reread saved properties etc.
//-------------------------------------------------------------------
void CObexFTPConnection::OnDeviceChange(IPropertyBag* /*Bag*/)
{
}

//-------------------------------------------------------------------
// CObexFTPConnection::GetDeviceName
// ask the device for its propertybag and the query the name
//-------------------------------------------------------------------
CString CObexFTPConnection::GetDeviceName()
{
  CObexDeviceProperties Prop(m_device);
  return Prop.GetName();
}

//-------------------------------------------------------------------
// CObexFTPConnection::Connect
// typically called in CObexFTP::OnDeviceArrived
// call CObexFTP::Connect to do the work
//-------------------------------------------------------------------
BOOL CObexFTPConnection::Connect(CObexFTP *pObj,IPropertyBag *Bag)
{
  if (pObj->Connect(this,Bag,&m_device))
  {
    m_pObj=pObj;
    return TRUE;
  }
  return FALSE;
}
//-------------------------------------------------------------------
// CObexFTPConnection::Disconnect
// call this function if you are done with the connection
// only delete a connection after Disconnect!
//-------------------------------------------------------------------
CObexFTPConnection *CObexFTPConnection::Disconnect(bool bAutoDel/*=false*/)
{
  if (m_device)
  {
    CComPtr<IHeaderCollection> Coll;
    Coll.CoCreateInstance(__uuidof(HeaderCollection));
    m_device->Disconnect(Coll);
    m_device=NULL;
  }
  if (m_pObj)
  {
    CObexFTP *pParent=m_pObj;
    m_pObj=NULL;
    pParent->Disconnect(this);
  }
  if (bAutoDel)
  {
    delete this;
    return NULL;
  }
  return this;
}

//-------------------------------------------------------------------
// CObexFTPConnection::SetPath - set the Server-Path
// NOTE: the server path is always relative so pszPath cannot contain
//       \\ characters! you can use NULL to go back to the root or use
//       dwFlags=SETPATH_FLAG_BACKUP to simulate a cd ..
//       use dwFlags=SETPATH_FLAG_DONT_CREATE if you only want to read
//-------------------------------------------------------------------
BOOL CObexFTPConnection::SetPath(LPCWSTR pszPath,DWORD dwFlags/*=0*/)
{
  if (m_device)
    return SUCCEEDED(m_device->SetPath(pszPath,dwFlags,NULL));
  else
    return FALSE;
}
//-------------------------------------------------------------------
// CObexFTPConnection::GetFile - get a file from the current server
//       path and store it in a Memory file
//       use SetPath before for nested folders
//-------------------------------------------------------------------
BOOL CObexFTPConnection::GetFile(LPCWSTR pszName,CStreamToMemory &Mem)
{
  if (m_device)
  {
    return GetFileOrDir(pszName,NULL,Mem);
  }
  else
  {
    return FALSE;
  }
}
//-------------------------------------------------------------------
// CObexFTPConnection::PutFile - send data as a file to the current 
//       server path 
//       use SetPath before for nested folders
//-------------------------------------------------------------------
BOOL CObexFTPConnection::PutFile(LPCWSTR pszName,void *Data,UINT Size)
{
  if (m_device)
  {
    CComPtr<IHeaderCollection> Coll;
    if (SUCCEEDED(Coll.CoCreateInstance(__uuidof(HeaderCollection))))
    {
      if (SUCCEEDED(Coll->AddName(pszName)) &&
          SUCCEEDED(Coll->AddLength(Size)))
      {
        CComPtr<IStream> Stream;
        if (SUCCEEDED(m_device->Put(Coll,&Stream)))
        {
          DWORD Written=0;
          if (SUCCEEDED(Stream->Write(Data,Size,&Written)) &&
              SUCCEEDED(Stream->Commit(0)))
          {
            return TRUE;
          }
        }
      }
    }
  }
  return FALSE;
}
//-------------------------------------------------------------------
// CObexFTPConnection::GetDirectory - retrieve the content of the
//       current server path 
//       use SetPath before for nested folders
//       see member FindFirstFile, FindNextFile of CObexListing
//-------------------------------------------------------------------
BOOL CObexFTPConnection::GetDirectory(CObexListing &Dir)
{
  if (m_device)
  {
    return GetFileOrDir(_T(""),"x-obex/folder-listing",Dir);
  }
  else
  {
    return FALSE;
  }
}
//-------------------------------------------------------------------
// CObexFTPConnection::GetFileOrDir 
//    helper function for GetFile and GetDirectory
//-------------------------------------------------------------------
BOOL CObexFTPConnection::GetFileOrDir(LPCWSTR pszName,const char *mime,CStreamToMemory &Mem)
{
  ATLASSERT(m_device);
  CComPtr<IHeaderCollection> Coll;
  if (SUCCEEDED(Coll.CoCreateInstance(__uuidof(HeaderCollection))))
  {
    if (SUCCEEDED(Coll->AddName(pszName)))
    {
      int mime_len=mime ? strlen(mime) : 0;
      if (mime_len==0 || SUCCEEDED(Coll->AddType(mime_len+1,(BYTE *)mime)))
      {
        CComPtr<IStream> Stream;
        if (SUCCEEDED(m_device->Get(Coll,&Stream)))
        {
          return Mem.Read(Stream);
        }
      }
    }
  }
  return FALSE;
}
