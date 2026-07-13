#pragma once

#include <string>

namespace ips {

struct DisplaySettings {
  bool showPhysicalSizeCm = true;
  bool showPhysicalWidthCm = false;
  bool showPhysicalHeightCm = false;
  bool showEmbeddedDpiX = false;
  bool showEmbeddedDpiY = false;
  bool showDpiSource = true;
  bool showDpiStatus = true;
  bool fallbackDpiEnabled = false;
  double fallbackDpi = 72.0;
  unsigned int maxDecimalPlaces = 2;
  bool trimTrailingZeros = true;
};

DisplaySettings LoadDisplaySettings();
bool IsKnownSettingName(const std::wstring& name);

}
