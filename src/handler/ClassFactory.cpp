#include "handler/ClassFactory.hpp"

#include "handler/Module.hpp"
#include "handler/PhysicalSizePropertyHandler.hpp"

#include <atomic>
#include <new>

namespace ips::handler {

ClassFactory::ClassFactory() {
  IncrementObjectCount();
}

ClassFactory::~ClassFactory() {
  DecrementObjectCount();
}

IFACEMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppv) {
  if (!ppv) {
    return E_POINTER;
  }
  *ppv = nullptr;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
    *ppv = static_cast<IClassFactory*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) ClassFactory::AddRef() {
  return refCount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

IFACEMETHODIMP_(ULONG) ClassFactory::Release() {
  const ULONG count = refCount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
  if (count == 0) {
    delete this;
  }
  return count;
}

IFACEMETHODIMP ClassFactory::CreateInstance(IUnknown* outer, REFIID riid, void** ppv) {
  if (!ppv) {
    return E_POINTER;
  }
  *ppv = nullptr;
  if (outer) {
    return CLASS_E_NOAGGREGATION;
  }

  auto* handler = new (std::nothrow) PhysicalSizePropertyHandler();
  if (!handler) {
    return E_OUTOFMEMORY;
  }

  HRESULT hr = handler->QueryInterface(riid, ppv);
  handler->Release();
  return hr;
}

IFACEMETHODIMP ClassFactory::LockServer(BOOL lock) {
  if (lock) {
    IncrementObjectCount();
  } else {
    DecrementObjectCount();
  }
  return S_OK;
}

}

