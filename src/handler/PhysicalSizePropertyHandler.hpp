#pragma once

#include "imaging/ImageInspection.hpp"
#include "common/Settings.hpp"

#include <windows.h>
#include <propsys.h>
#include <wrl/client.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace ips::handler {

class PhysicalSizePropertyHandler final : public IInitializeWithStream,
                                          public IInitializeWithFile,
                                          public IPropertyStore,
                                          public IPropertyStoreCapabilities {
 public:
  PhysicalSizePropertyHandler();
  PhysicalSizePropertyHandler(const PhysicalSizePropertyHandler&) = delete;
  PhysicalSizePropertyHandler& operator=(const PhysicalSizePropertyHandler&) = delete;

  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;

  IFACEMETHODIMP Initialize(IStream* stream, DWORD grfMode) override;
  IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

  IFACEMETHODIMP GetCount(DWORD* propertyCount) override;
  IFACEMETHODIMP GetAt(DWORD propertyIndex, PROPERTYKEY* key) override;
  IFACEMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* value) override;
  IFACEMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT value) override;
  IFACEMETHODIMP Commit() override;

  IFACEMETHODIMP IsPropertyWritable(REFPROPERTYKEY key) override;

 private:
  ~PhysicalSizePropertyHandler();

  HRESULT InitializeFromStream(IStream* stream, DWORD grfMode, std::wstring filePath);
  HRESULT InitializeOriginalHandler(IStream* stream, const std::wstring& filePath);
  HRESULT GetCustomValue(REFPROPERTYKEY key, PROPVARIANT* value);

  std::atomic<ULONG> refCount_ = 1;
  std::mutex mutex_;
  bool initialized_ = false;
  DWORD grfMode_ = STGM_READ;
  std::wstring filePath_;
  DisplaySettings settings_;
  ImageInspection inspection_;
  Microsoft::WRL::ComPtr<IPropertyStore> originalStore_;
  Microsoft::WRL::ComPtr<IPropertyStoreCapabilities> originalCapabilities_;
  std::vector<PROPERTYKEY> customKeys_;
};

}
