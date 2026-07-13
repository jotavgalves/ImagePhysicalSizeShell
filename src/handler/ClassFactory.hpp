#pragma once

#include <unknwn.h>

#include <atomic>

namespace ips::handler {

class ClassFactory final : public IClassFactory {
 public:
  ClassFactory();
  ClassFactory(const ClassFactory&) = delete;
  ClassFactory& operator=(const ClassFactory&) = delete;

  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;
  IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override;
  IFACEMETHODIMP LockServer(BOOL lock) override;

 private:
  ~ClassFactory();
  std::atomic<ULONG> refCount_ = 1;
};

}
