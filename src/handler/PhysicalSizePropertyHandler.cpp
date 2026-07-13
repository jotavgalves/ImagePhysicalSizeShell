#include "handler/PhysicalSizePropertyHandler.hpp"

#include "handler/ComIds.hpp"
#include "handler/Module.hpp"
#include "handler/OriginalHandler.hpp"
#include "handler/PropVariantHelpers.hpp"
#include "common/Settings.hpp"
#include "imaging/PhysicalSize.hpp"
#include "imaging/StreamImageInspection.hpp"
#include "property_schema/PropertyKeys.hpp"

#include <propkey.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <wrl/client.h>

namespace ips::handler {
namespace {

using Microsoft::WRL::ComPtr;

bool IsSameKey(REFPROPERTYKEY a, REFPROPERTYKEY b) {
  return IsEqualGUID(a.fmtid, b.fmtid) && a.pid == b.pid;
}

bool IsCustomProperty(REFPROPERTYKEY key) {
  return IsEqualGUID(key.fmtid, schema::kFormatIdImagePhysicalSize) && key.pid >= 2 && key.pid <= 8;
}

bool IsPropertyEnabled(REFPROPERTYKEY key, const DisplaySettings& settings) {
  if (IsSameKey(key, schema::PKEY_PhysicalWidthCm)) {
    return settings.showPhysicalWidthCm;
  }
  if (IsSameKey(key, schema::PKEY_PhysicalHeightCm)) {
    return settings.showPhysicalHeightCm;
  }
  if (IsSameKey(key, schema::PKEY_PhysicalSizeCm)) {
    return settings.showPhysicalSizeCm;
  }
  if (IsSameKey(key, schema::PKEY_EmbeddedDpiX)) {
    return settings.showEmbeddedDpiX;
  }
  if (IsSameKey(key, schema::PKEY_EmbeddedDpiY)) {
    return settings.showEmbeddedDpiY;
  }
  if (IsSameKey(key, schema::PKEY_DpiSource)) {
    return settings.showDpiSource;
  }
  if (IsSameKey(key, schema::PKEY_DpiStatus)) {
    return settings.showDpiStatus;
  }
  return false;
}

std::vector<PROPERTYKEY> BuildCustomKeys(const DisplaySettings& settings) {
  std::vector<PROPERTYKEY> keys;
  const std::vector<PROPERTYKEY> allKeys = {
      schema::PKEY_PhysicalWidthCm, schema::PKEY_PhysicalHeightCm, schema::PKEY_PhysicalSizeCm,
      schema::PKEY_EmbeddedDpiX,    schema::PKEY_EmbeddedDpiY,      schema::PKEY_DpiSource,
      schema::PKEY_DpiStatus,
  };
  for (const auto& key : allKeys) {
    if (IsPropertyEnabled(key, settings)) {
      keys.push_back(key);
    }
  }
  return keys;
}

NumberFormatOptions NumberOptionsFromSettings(const DisplaySettings& settings) {
  return NumberFormatOptions{settings.maxDecimalPlaces, settings.trimTrailingZeros};
}

void ApplyFallbackDpiIfConfigured(ImageInspection& inspection, const DisplaySettings& settings) {
  if (inspection.embeddedDpi.found || !settings.fallbackDpiEnabled || !IsReasonableDpi(settings.fallbackDpi)) {
    return;
  }

  auto physicalSize =
      CalculatePhysicalSizeCm(inspection.widthPixels, inspection.heightPixels, settings.fallbackDpi, settings.fallbackDpi);
  if (!physicalSize) {
    return;
  }

  inspection.embeddedDpi.found = true;
  inspection.embeddedDpi.dpiX = settings.fallbackDpi;
  inspection.embeddedDpi.dpiY = settings.fallbackDpi;
  inspection.embeddedDpi.source = L"DPI inferido";
  inspection.embeddedDpi.status = L"DPI inferido";
  inspection.hasPhysicalSize = true;
  inspection.widthCm = physicalSize->widthCm;
  inspection.heightCm = physicalSize->heightCm;
}

HRESULT CloneStreamAtStart(IStream* stream, IStream** clone) {
  if (!stream || !clone) {
    return E_POINTER;
  }
  *clone = nullptr;
  HRESULT hr = stream->Clone(clone);
  if (FAILED(hr)) {
    return hr;
  }
  LARGE_INTEGER zero{};
  (*clone)->Seek(zero, STREAM_SEEK_SET, nullptr);
  return S_OK;
}

HRESULT CreateOriginalInstance(const CLSID& clsid, IStream* stream, const std::wstring& filePath, IPropertyStore** store,
                               IPropertyStoreCapabilities** capabilities) {
  if (!stream || !store) {
    return E_POINTER;
  }
  *store = nullptr;
  if (capabilities) {
    *capabilities = nullptr;
  }

  ComPtr<IUnknown> unknown;
  HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&unknown));
  if (FAILED(hr)) {
    return hr;
  }

  ComPtr<IInitializeWithStream> initializer;
  hr = unknown.As(&initializer);
  if (SUCCEEDED(hr)) {
    ComPtr<IStream> originalStream;
    hr = CloneStreamAtStart(stream, originalStream.GetAddressOf());
    if (FAILED(hr) && !filePath.empty()) {
      hr = SHCreateStreamOnFileEx(filePath.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE,
                                  nullptr, originalStream.GetAddressOf());
    }
    if (FAILED(hr)) {
      return hr;
    }

    hr = initializer->Initialize(originalStream.Get(), STGM_READ);
    if (SUCCEEDED(hr)) {
      hr = S_OK;
    } else if (filePath.empty()) {
      return hr;
    }
  }

  if (FAILED(hr) && !filePath.empty()) {
    ComPtr<IInitializeWithFile> fileInitializer;
    hr = unknown.As(&fileInitializer);
    if (SUCCEEDED(hr)) {
      hr = fileInitializer->Initialize(filePath.c_str(), STGM_READ);
    }
  }

  if (FAILED(hr) && !filePath.empty()) {
    ComPtr<IInitializeWithItem> itemInitializer;
    hr = unknown.As(&itemInitializer);
    if (SUCCEEDED(hr)) {
      ComPtr<IShellItem> item;
      hr = SHCreateItemFromParsingName(filePath.c_str(), nullptr, IID_PPV_ARGS(&item));
      if (SUCCEEDED(hr)) {
        hr = itemInitializer->Initialize(item.Get(), STGM_READ);
      }
    }
  }

  if (FAILED(hr)) {
    return hr;
  }

  ComPtr<IPropertyStore> propertyStore;
  hr = unknown.As(&propertyStore);
  if (FAILED(hr)) {
    return hr;
  }

  *store = propertyStore.Detach();
  if (capabilities) {
    ComPtr<IPropertyStoreCapabilities> caps;
    if (SUCCEEDED(unknown.As(&caps))) {
      *capabilities = caps.Detach();
    }
  }

  return S_OK;
}

}

PhysicalSizePropertyHandler::PhysicalSizePropertyHandler() {
  IncrementObjectCount();
}

PhysicalSizePropertyHandler::~PhysicalSizePropertyHandler() {
  DecrementObjectCount();
}

IFACEMETHODIMP PhysicalSizePropertyHandler::QueryInterface(REFIID riid, void** ppv) {
  if (!ppv) {
    return E_POINTER;
  }
  *ppv = nullptr;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IInitializeWithStream)) {
    *ppv = static_cast<IInitializeWithStream*>(this);
  } else if (IsEqualIID(riid, IID_IInitializeWithFile)) {
    *ppv = static_cast<IInitializeWithFile*>(this);
  } else if (IsEqualIID(riid, IID_IPropertyStore)) {
    *ppv = static_cast<IPropertyStore*>(this);
  } else if (IsEqualIID(riid, IID_IPropertyStoreCapabilities)) {
    *ppv = static_cast<IPropertyStoreCapabilities*>(this);
  } else {
    return E_NOINTERFACE;
  }

  AddRef();
  return S_OK;
}

IFACEMETHODIMP_(ULONG) PhysicalSizePropertyHandler::AddRef() {
  return refCount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

IFACEMETHODIMP_(ULONG) PhysicalSizePropertyHandler::Release() {
  const ULONG count = refCount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
  if (count == 0) {
    delete this;
  }
  return count;
}

IFACEMETHODIMP PhysicalSizePropertyHandler::Initialize(IStream* stream, DWORD grfMode) {
  return InitializeFromStream(stream, grfMode, L"");
}

IFACEMETHODIMP PhysicalSizePropertyHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode) {
  if (!pszFilePath || !*pszFilePath) {
    return E_INVALIDARG;
  }

  ComPtr<IStream> stream;
  HRESULT hr = SHCreateStreamOnFileEx(pszFilePath, STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE,
                                      nullptr, stream.GetAddressOf());
  if (FAILED(hr)) {
    hr = SHCreateStreamOnFileEx(pszFilePath, STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr,
                                stream.GetAddressOf());
  }
  if (FAILED(hr)) {
    return hr;
  }

  return InitializeFromStream(stream.Get(), grfMode, pszFilePath);
}

HRESULT PhysicalSizePropertyHandler::InitializeFromStream(IStream* stream, DWORD grfMode, std::wstring filePath) {
  if (!stream) {
    return E_POINTER;
  }

  std::lock_guard lock(mutex_);
  if (initialized_) {
    return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
  }

  grfMode_ = grfMode;
  filePath_ = std::move(filePath);
  settings_ = LoadDisplaySettings();
  customKeys_ = BuildCustomKeys(settings_);

  std::wstring inspectionError;
  if (!InspectImageStream(stream, inspection_, inspectionError)) {
    return E_FAIL;
  }
  ApplyFallbackDpiIfConfigured(inspection_, settings_);

  HRESULT hr = InitializeOriginalHandler(stream, filePath_);
  if (FAILED(hr)) {
    return hr;
  }

  initialized_ = true;
  return S_OK;
}

HRESULT PhysicalSizePropertyHandler::InitializeOriginalHandler(IStream* stream, const std::wstring& filePath) {
  const std::wstring canonicalExtension = CanonicalExtensionForFormatName(FormatName(inspection_.format));
  if (canonicalExtension.empty()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
  }

  const auto originalClsid = ReadSavedOriginalHandlerClsid(canonicalExtension);
  if (!originalClsid) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }
  if (IsEqualGUID(*originalClsid, CLSID_ImagePhysicalSizePropertyHandler)) {
    return HRESULT_FROM_WIN32(ERROR_REPARSE_TAG_INVALID);
  }

  return CreateOriginalInstance(*originalClsid, stream, filePath, originalStore_.GetAddressOf(),
                                originalCapabilities_.GetAddressOf());
}

IFACEMETHODIMP PhysicalSizePropertyHandler::GetCount(DWORD* propertyCount) {
  if (!propertyCount) {
    return E_POINTER;
  }

  std::lock_guard lock(mutex_);
  if (!initialized_ || !originalStore_) {
    return E_UNEXPECTED;
  }

  DWORD originalCount = 0;
  HRESULT hr = originalStore_->GetCount(&originalCount);
  if (FAILED(hr)) {
    return hr;
  }

  *propertyCount = originalCount + static_cast<DWORD>(customKeys_.size());
  return S_OK;
}

IFACEMETHODIMP PhysicalSizePropertyHandler::GetAt(DWORD propertyIndex, PROPERTYKEY* key) {
  if (!key) {
    return E_POINTER;
  }

  std::lock_guard lock(mutex_);
  if (!initialized_ || !originalStore_) {
    return E_UNEXPECTED;
  }

  DWORD originalCount = 0;
  HRESULT hr = originalStore_->GetCount(&originalCount);
  if (FAILED(hr)) {
    return hr;
  }

  if (propertyIndex < originalCount) {
    return originalStore_->GetAt(propertyIndex, key);
  }

  const DWORD customIndex = propertyIndex - originalCount;
  if (customIndex >= customKeys_.size()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  *key = customKeys_[customIndex];
  return S_OK;
}

IFACEMETHODIMP PhysicalSizePropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT* value) {
  if (!value) {
    return E_POINTER;
  }

  std::lock_guard lock(mutex_);
  if (!initialized_ || !originalStore_) {
    return E_UNEXPECTED;
  }

  if (IsCustomProperty(key)) {
    if (!IsPropertyEnabled(key, settings_)) {
      return InitPropVariantEmpty(value);
    }
    return GetCustomValue(key, value);
  }

  return originalStore_->GetValue(key, value);
}

HRESULT PhysicalSizePropertyHandler::GetCustomValue(REFPROPERTYKEY key, PROPVARIANT* value) {
  if (IsSameKey(key, schema::PKEY_PhysicalWidthCm)) {
    return inspection_.hasPhysicalSize ? InitPropVariantFromDoubleValue(RoundToDecimalPlaces(inspection_.widthCm, settings_.maxDecimalPlaces), value)
                                       : InitPropVariantEmpty(value);
  }
  if (IsSameKey(key, schema::PKEY_PhysicalHeightCm)) {
    return inspection_.hasPhysicalSize ? InitPropVariantFromDoubleValue(RoundToDecimalPlaces(inspection_.heightCm, settings_.maxDecimalPlaces), value)
                                       : InitPropVariantEmpty(value);
  }
  if (IsSameKey(key, schema::PKEY_PhysicalSizeCm)) {
    if (!inspection_.hasPhysicalSize) {
      return InitPropVariantFromStringValue(L"DPI não incorporado", value);
    }
    return InitPropVariantFromStringValue(
        FormatPhysicalSizeCm(inspection_.widthCm, inspection_.heightCm, NumberOptionsFromSettings(settings_)), value);
  }
  if (IsSameKey(key, schema::PKEY_EmbeddedDpiX)) {
    return inspection_.embeddedDpi.found ? InitPropVariantFromDoubleValue(RoundToDecimalPlaces(inspection_.embeddedDpi.dpiX, settings_.maxDecimalPlaces), value)
                                         : InitPropVariantEmpty(value);
  }
  if (IsSameKey(key, schema::PKEY_EmbeddedDpiY)) {
    return inspection_.embeddedDpi.found ? InitPropVariantFromDoubleValue(RoundToDecimalPlaces(inspection_.embeddedDpi.dpiY, settings_.maxDecimalPlaces), value)
                                         : InitPropVariantEmpty(value);
  }
  if (IsSameKey(key, schema::PKEY_DpiSource)) {
    return InitPropVariantFromStringValue(inspection_.embeddedDpi.source, value);
  }
  if (IsSameKey(key, schema::PKEY_DpiStatus)) {
    return InitPropVariantFromStringValue(inspection_.embeddedDpi.status, value);
  }

  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

IFACEMETHODIMP PhysicalSizePropertyHandler::SetValue(REFPROPERTYKEY key, REFPROPVARIANT value) {
  UNREFERENCED_PARAMETER(value);

  std::lock_guard lock(mutex_);
  if (!initialized_ || !originalStore_) {
    return E_UNEXPECTED;
  }

  if (IsCustomProperty(key)) {
    return STG_E_ACCESSDENIED;
  }

  return originalStore_->SetValue(key, value);
}

IFACEMETHODIMP PhysicalSizePropertyHandler::Commit() {
  std::lock_guard lock(mutex_);
  if (!initialized_ || !originalStore_) {
    return E_UNEXPECTED;
  }
  return originalStore_->Commit();
}

IFACEMETHODIMP PhysicalSizePropertyHandler::IsPropertyWritable(REFPROPERTYKEY key) {
  std::lock_guard lock(mutex_);
  if (IsCustomProperty(key)) {
    return S_FALSE;
  }
  if (originalCapabilities_) {
    return originalCapabilities_->IsPropertyWritable(key);
  }
  return S_FALSE;
}

}
