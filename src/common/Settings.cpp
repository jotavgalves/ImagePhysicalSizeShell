#include "common/Settings.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>

namespace ips {
namespace {

constexpr wchar_t kSettingsPath[] = L"Software\\ImagePhysicalSizeShell\\Settings";

bool ReadDword(HKEY key, const wchar_t* name, bool defaultValue) {
  DWORD value = defaultValue ? 1u : 0u;
  DWORD type = 0;
  DWORD size = sizeof(value);
  if (RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, &type, &value, &size) != ERROR_SUCCESS) {
    return defaultValue;
  }
  return value != 0;
}

double ReadDoubleFromDword(HKEY key, const wchar_t* name, double defaultValue) {
  DWORD value = static_cast<DWORD>(defaultValue);
  DWORD type = 0;
  DWORD size = sizeof(value);
  if (RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, &type, &value, &size) != ERROR_SUCCESS) {
    return defaultValue;
  }
  return static_cast<double>(value);
}

unsigned int ReadDwordClamped(HKEY key, const wchar_t* name, unsigned int defaultValue, unsigned int minValue,
                              unsigned int maxValue) {
  DWORD value = defaultValue;
  DWORD type = 0;
  DWORD size = sizeof(value);
  if (RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, &type, &value, &size) != ERROR_SUCCESS) {
    return defaultValue;
  }
  return std::clamp(static_cast<unsigned int>(value), minValue, maxValue);
}

}

DisplaySettings LoadDisplaySettings() {
  DisplaySettings settings;

  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
    return settings;
  }

  settings.showPhysicalSizeCm = ReadDword(key, L"ShowPhysicalSizeCm", settings.showPhysicalSizeCm);
  settings.showPhysicalWidthCm = ReadDword(key, L"ShowPhysicalWidthCm", settings.showPhysicalWidthCm);
  settings.showPhysicalHeightCm = ReadDword(key, L"ShowPhysicalHeightCm", settings.showPhysicalHeightCm);
  settings.showEmbeddedDpiX = ReadDword(key, L"ShowEmbeddedDpiX", settings.showEmbeddedDpiX);
  settings.showEmbeddedDpiY = ReadDword(key, L"ShowEmbeddedDpiY", settings.showEmbeddedDpiY);
  settings.showDpiSource = ReadDword(key, L"ShowDpiSource", settings.showDpiSource);
  settings.showDpiStatus = ReadDword(key, L"ShowDpiStatus", settings.showDpiStatus);
  settings.fallbackDpiEnabled = ReadDword(key, L"FallbackDpiEnabled", settings.fallbackDpiEnabled);
  settings.fallbackDpi = ReadDoubleFromDword(key, L"FallbackDpi", settings.fallbackDpi);
  settings.maxDecimalPlaces = ReadDwordClamped(key, L"MaxDecimalPlaces", settings.maxDecimalPlaces, 0, 6);
  settings.trimTrailingZeros = ReadDword(key, L"TrimTrailingZeros", settings.trimTrailingZeros);

  if (!std::isfinite(settings.fallbackDpi) || settings.fallbackDpi <= 0.0 || settings.fallbackDpi > 100000.0) {
    settings.fallbackDpi = 72.0;
  }

  RegCloseKey(key);
  return settings;
}

bool IsKnownSettingName(const std::wstring& name) {
  return name == L"ShowPhysicalSizeCm" || name == L"ShowPhysicalWidthCm" ||
         name == L"ShowPhysicalHeightCm" || name == L"ShowEmbeddedDpiX" ||
         name == L"ShowEmbeddedDpiY" || name == L"ShowDpiSource" || name == L"ShowDpiStatus" ||
         name == L"FallbackDpiEnabled" || name == L"FallbackDpi" || name == L"MaxDecimalPlaces" ||
         name == L"TrimTrailingZeros";
}

}
