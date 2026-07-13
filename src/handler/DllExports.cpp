#include "handler/ClassFactory.hpp"
#include "handler/ComIds.hpp"
#include "handler/Module.hpp"

#include <windows.h>

#include <new>

using ips::handler::CLSID_ImagePhysicalSizePropertyHandler;

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
  if (reason == DLL_PROCESS_ATTACH) {
    ips::handler::SetModuleHandle(instance);
    DisableThreadLibraryCalls(instance);
  }
  return TRUE;
}

STDAPI DllCanUnloadNow() {
  return ips::handler::CanUnloadNow() ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* ppv) {
  if (!ppv) {
    return E_POINTER;
  }
  *ppv = nullptr;
  if (!IsEqualCLSID(clsid, CLSID_ImagePhysicalSizePropertyHandler)) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }

  auto* factory = new (std::nothrow) ips::handler::ClassFactory();
  if (!factory) {
    return E_OUTOFMEMORY;
  }
  HRESULT hr = factory->QueryInterface(riid, ppv);
  factory->Release();
  return hr;
}

STDAPI DllRegisterServer() {
  return E_NOTIMPL;
}

STDAPI DllUnregisterServer() {
  return E_NOTIMPL;
}
