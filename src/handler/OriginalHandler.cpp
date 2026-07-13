#include "handler/OriginalHandler.hpp"

#include <objbase.h>
#include <winreg.h>

namespace ips::handler {
namespace {

constexpr wchar_t kOriginalHandlersRoot[] = L"SOFTWARE\\ImagePhysicalSizeShell\\OriginalHandlers";
constexpr wchar_t kOriginalClsidValue[] = L"OriginalClsid";

}

std::optional<CLSID> ReadSavedOriginalHandlerClsid(const std::wstring& canonicalExtension) {
  const std::wstring path = std::wstring(kOriginalHandlersRoot) + L"\\" + canonicalExtension;

  HKEY key = nullptr;
  LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key);
  if (status != ERROR_SUCCESS) {
    return std::nullopt;
  }

  wchar_t buffer[64] = {};
  DWORD type = REG_SZ;
  DWORD bytes = sizeof(buffer);
  status = RegQueryValueExW(key, kOriginalClsidValue, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &bytes);
  RegCloseKey(key);

  if (status != ERROR_SUCCESS || type != REG_SZ) {
    return std::nullopt;
  }

  CLSID clsid{};
  if (FAILED(CLSIDFromString(buffer, &clsid))) {
    return std::nullopt;
  }

  return clsid;
}

std::wstring CanonicalExtensionForFormatName(const std::wstring& formatName) {
  if (formatName == L"PNG") {
    return L".png";
  }
  if (formatName == L"JPEG") {
    return L".jpg";
  }
  if (formatName == L"TIFF") {
    return L".tif";
  }
  return L"";
}

}
