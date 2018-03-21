#ifndef _OBEX_UTIL_H_
#define _OBEX_UTIL_H_

#include <obex.h>

//-------------------------------------------------------------------
// Helper class for easy access to ObexDevice Properties
//-------------------------------------------------------------------
class CObexDeviceProperties
{
public:
  CObexDeviceProperties(IPropertyBag *Prop);
  CObexDeviceProperties(IObexDevice *Dev);
  CObexDeviceProperties(IUnknown *Unk);
  ~CObexDeviceProperties();

  // Basic call to read any property
  HRESULT GetProperty(LPCOLESTR PropName,VARIANT &Value);
  CString GetProperty(LPCOLESTR PropName);
  // Retrieve known properties
  CString GetName();    // NAME of the Device
  CString GetAddress(); // ADDRESS of the device
  CString GetTransport(); // TRANSPORT of the device
  CString GetServices();  // SERVICEs of the device
  bool CanFTP();          // OBEX-FTP supported?
  // raw pointer for the PropertyBag
	operator IPropertyBag*() const
	{
		return (IPropertyBag*)m_Bag;
	}
protected:
  CComPtr<IPropertyBag> m_Bag;
};

//-------------------------------------------------------------------
// template for a simple list of objects with a m_pNext member
// the list can be searched if the T has a member IsKey()
//-------------------------------------------------------------------
template <class T> class CSimpleList
{
public:
  T * m_pHead;

  CSimpleList() : m_pHead(NULL)
  {
  }
  ~CSimpleList()
  {
    RemoveAll();
  }

  // Remove all elements (delete objects)
  void RemoveAll()
  {
    while (!IsEmpty())
      Remove(m_pHead);
    m_pHead=NULL;
  }

  // Test if any elements in list
  BOOL IsEmpty()
  {
    return m_pHead==NULL;
  }

  // Unlink an element, but do not delete
  // returns pointer to element or NULL
  T *Unlink(T *Arg)
  {
    if (Arg && m_pHead)
    {
      if (Arg==m_pHead)
      {
        m_pHead=Arg->m_pNext;
        return Arg;
      }
      else 
      {
        T *p=m_pHead;
        while (p && p->m_pNext!=Arg)
          p=p->m_pNext;
        if (p)
        {
          p->m_pNext=Arg->m_pNext;
          return Arg;
        }
      }
    }
    return NULL;
  }

  // Unlink and delete an element
  BOOL Remove(T *Arg)
  {
    Arg=Unlink(Arg);
    if (Arg)
      delete Arg;
    return Arg!=NULL;
  }

  // Add an element to end of list
  T * Append(T *Arg)
  {
    if (Arg)
    {
      Arg->m_pNext=NULL;
      if (IsEmpty())
        m_pHead=Arg;
      else
      {
        T * p=m_pHead;
        while (p->m_pNext)
          p=p->m_pNext;
        p->m_pNext=Arg;
      }
    }
    return Arg;
  }
  // Add an element to top of list
  T * Push(T *Arg)
  {
    if (Arg)
    {
      Arg->m_pNext=m_pHead;
      m_pHead=Arg;
    }
    return Arg;
  }

  // Find an Element in List
  T * Find(LPCWSTR Key)
  {
    T *p=m_pHead;
    while (p && !p->IsKey(Key))
      p=p->m_pNext;
    return p;
  }
};

//-------------------------------------------------------------------
// Simple wrapper to read an IStream * into memory
//-------------------------------------------------------------------
class CStreamToMemory
{
public:
  CStreamToMemory(UINT Initial=0,UINT Add=4096);
  ~CStreamToMemory();
  BOOL Read(IStream *pStream);
  UINT Size();
  BYTE *GetData();
  void Clear();
  void Reset();
protected:
  BYTE *m_Buffer;
  UINT m_Allocated;
  UINT m_Filled;
  UINT m_PageSize;
};
//-------------------------------------------------------------------
// CStreamToMemory derived class to manage x-obex/file-list
//-------------------------------------------------------------------
class CObexListing : public CStreamToMemory
{
public:
  CObexListing(UINT Initial=0,UINT Add=4096);
  ~CObexListing();
  BOOL FindFirstFile(WIN32_FIND_DATA *FindFileData,UINT Mask=FILE_ATTRIBUTE_NORMAL|FILE_ATTRIBUTE_DIRECTORY);
  BOOL FindNextFile(WIN32_FIND_DATA *FindFileData);
  BOOL FindClose();
protected:
  bool AtText(const char *t);
  bool IsProp(const char *t,char **pAt);
  void NextProp();
  void SkipProp();
  void GetDateProp(const char *At,FILETIME &ft);
  char *Skip(const char *t);
protected:
  char *m_pCur;
  UINT  m_Mask;
};
#endif
