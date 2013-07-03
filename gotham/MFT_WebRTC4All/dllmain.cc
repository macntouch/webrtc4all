/*
* Copyright (C) 2013 Gotham City. All rights reserved.
* Copyright (C) Microsoft Corporation. All rights reserved.
*/
/**@file dllmain.cc
 * @brief Implementation for DLL exports and COM class factory for registration. 
 * If 'Regsvr32.exe' works it's maily because of this.
 *
 * @author Batman@GothamCity
 */
#include <assert.h>
#include <new>
#include <shlwapi.h>

#include "w4all.h"

#include <initguid.h>
#include "w4allGuids.h" /* Must be here (after 'initguid.h') */


// Forward declarations
extern HRESULT CW4allSink_CreateInstance(REFIID iid, void **ppMFT); /* implementation: w4allSink.cc */
extern HRESULT CW4allSource_CreateInstance(REFIID iid, void **ppMFT); /* implementation: w4allSource.cc */

HRESULT RegisterObject(
    HMODULE hModule,
    const GUID& guid,
    const TCHAR *pszDescription,
    const TCHAR *pszThreadingModel
    );

HRESULT UnregisterObject(const GUID& guid);


// Module Ref count
long g_cRefModule = 0;

// Handle to the DLL's module
HMODULE g_hModule = NULL;

void DllAddRef()
{
    InterlockedIncrement(&g_cRefModule);
}

void DllRelease()
{
    InterlockedDecrement(&g_cRefModule);
}

//
// IClassFactory implementation
//

typedef HRESULT (*PFNCREATEINSTANCE)(REFIID riid, void **ppvObject);
struct CLASS_OBJECT_INIT
{
    const CLSID *pClsid;
    PFNCREATEINSTANCE pfnCreate;
};

// Classes supported by this module:
const CLASS_OBJECT_INIT c_rgClassObjectInit[] =
{
	{ &CLSID_WebRTC4AllSourceMFT, CW4allSource_CreateInstance },
	{ &CLSID_WebRTC4AllSinkMFT, CW4allSink_CreateInstance },
	/* add here other modules */
};

class CClassFactory : public IClassFactory
{
public:

    static HRESULT CreateInstance(
        REFCLSID clsid,                                 // The CLSID of the object to create (from DllGetClassObject)
        const CLASS_OBJECT_INIT *pClassObjectInits,     // Array of class factory data.
        size_t cClassObjectInits,                       // Number of elements in the array.
        REFIID riid,                                    // The IID of the interface to retrieve (from DllGetClassObject)
        void **ppv                                      // Receives a pointer to the interface.
        )
    {
        *ppv = NULL;

        HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;

        for (size_t i = 0; i < cClassObjectInits; i++)
        {
            if (clsid == *pClassObjectInits[i].pClsid)
            {
                IClassFactory *pClassFactory = new (std::nothrow) CClassFactory(pClassObjectInits[i].pfnCreate);

                if (pClassFactory)
                {
                    hr = pClassFactory->QueryInterface(riid, ppv);
                    pClassFactory->Release();
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                }
                break; // match found
            }
        }
        return hr;
    }

    // IUnknown methods
    IFACEMETHODIMP QueryInterface(REFIID riid, void ** ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CClassFactory, IClassFactory),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        long cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
        {
            delete this;
        }
        return cRef;
    }

    // IClassFactory methods

    IFACEMETHODIMP CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
    {
        return punkOuter ? CLASS_E_NOAGGREGATION : m_pfnCreate(riid, ppv);
    }

    IFACEMETHODIMP LockServer(BOOL fLock)
    {
        if (fLock)
        {
            DllAddRef();
        }
        else
        {
            DllRelease();
        }
        return S_OK;
    }

private:

    CClassFactory(PFNCREATEINSTANCE pfnCreate) : m_cRef(1), m_pfnCreate(pfnCreate)
    {
        DllAddRef();
    }

    ~CClassFactory()
    {
        DllRelease();
    }

    long m_cRef;
    PFNCREATEINSTANCE m_pfnCreate;
};



//
// Standard DLL functions
//

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void *)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hModule = (HMODULE)hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    return (g_cRefModule == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv)
{
    return CClassFactory::CreateInstance(clsid, c_rgClassObjectInit, ARRAYSIZE(c_rgClassObjectInit), riid, ppv);
}



STDAPI DllRegisterServer()
{
    assert(g_hModule != NULL);

    // Register the CLSID for CoCreateInstance.
	HRESULT hr =  RegisterObject(g_hModule, CLSID_WebRTC4AllSourceMFT, TEXT("Doubango Telecom WebRTC4All audio/video Source"), TEXT("Both"));
	if(SUCCEEDED(hr))
	{
		hr = RegisterObject(g_hModule, CLSID_WebRTC4AllSinkMFT, TEXT("Doubango Telecom WebRTC4All audio/video Sink"), TEXT("Both"));
	}
	return hr;
}

STDAPI DllUnregisterServer()
{
    // Unregister the CLSID.
    UnregisterObject(CLSID_WebRTC4AllSinkMFT);
	UnregisterObject(CLSID_WebRTC4AllSourceMFT);

    return S_OK;
}





// Converts a CLSID into a string with the form "CLSID\{clsid}"
HRESULT CreateObjectKeyName(const GUID& guid, TCHAR *sName, DWORD cchMax)
{
    const DWORD CHARS_IN_GUID = 39;

    // convert CLSID uuid to string
    OLECHAR szCLSID[CHARS_IN_GUID];
    HRESULT hr = StringFromGUID2(guid, szCLSID, CHARS_IN_GUID);
    if (SUCCEEDED(hr))
    {
        // Create a string of the form "CLSID\{clsid}"
        hr = StringCchPrintf(sName, cchMax, TEXT("Software\\Classes\\CLSID\\%ls"), szCLSID);
    }
    return hr;
}

// Creates a registry key (if needed) and sets the default value of the key
HRESULT CreateRegKeyAndValue(
    HKEY hKey,
    PCWSTR pszSubKeyName,
    PCWSTR pszValueName,
    PCWSTR pszData,
    PHKEY phkResult
    )
{
    *phkResult = NULL;

    LONG lRet = RegCreateKeyEx(
        hKey, pszSubKeyName,
        0,  NULL, REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS, NULL, phkResult, NULL);

    if (lRet == ERROR_SUCCESS)
    {
        lRet = RegSetValueExW(
            (*phkResult),
            pszValueName, 0, REG_SZ,
            (LPBYTE) pszData,
            ((DWORD) wcslen(pszData) + 1) * sizeof(WCHAR)
            );

        if (lRet != ERROR_SUCCESS)
        {
            RegCloseKey(*phkResult);
        }
    }

    return HRESULT_FROM_WIN32(lRet);
}

// Creates the registry entries for a COM object.

HRESULT RegisterObject(
    HMODULE hModule,
    const GUID& guid,
    const TCHAR *pszDescription,
    const TCHAR *pszThreadingModel
    )
{
    HKEY hKey = NULL;
    HKEY hSubkey = NULL;

    TCHAR achTemp[MAX_PATH];

    // Create the name of the key from the object's CLSID
    HRESULT hr = CreateObjectKeyName(guid, achTemp, MAX_PATH);

    // Create the new key.
    if (SUCCEEDED(hr))
    {
        hr = CreateRegKeyAndValue(
            HKEY_LOCAL_MACHINE,
            achTemp,
            NULL,
            pszDescription,
            &hKey
            );
    }

    if (SUCCEEDED(hr))
    {
        (void)GetModuleFileName(hModule, achTemp, MAX_PATH);

        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    // Create the "InprocServer32" subkey
    if (SUCCEEDED(hr))
    {
        hr = CreateRegKeyAndValue(
            hKey,
            L"InProcServer32",
            NULL,
            achTemp,
            &hSubkey
            );

        RegCloseKey(hSubkey);
    }

    // Add a new value to the subkey, for "ThreadingModel" = <threading model>
    if (SUCCEEDED(hr))
    {
        hr = CreateRegKeyAndValue(
            hKey,
            L"InProcServer32",
            L"ThreadingModel",
            pszThreadingModel,
            &hSubkey
            );

        RegCloseKey(hSubkey);
    }

    // close hkeys

    RegCloseKey(hKey);

    return hr;
}

// Deletes the registry entries for a COM object.

HRESULT UnregisterObject(const GUID& guid)
{
    TCHAR achTemp[MAX_PATH];

    HRESULT hr = CreateObjectKeyName(guid, achTemp, MAX_PATH);

    if (SUCCEEDED(hr))
    {
        // Delete the key recursively.
        LONG lRes = RegDeleteTree(HKEY_LOCAL_MACHINE, achTemp);

        hr = HRESULT_FROM_WIN32(lRes);
    }

    return hr;
}


// Memory allocation function for RPC.
// The runtime uses these two functions for allocating/deallocating
// enough memory to pass the string to the server.
void* __RPC_USER midl_user_allocate(size_t size)
{
    return malloc(size);
}

// Memory deallocation function for RPC.
void __RPC_USER midl_user_free(void* p)
{
    free(p);
}