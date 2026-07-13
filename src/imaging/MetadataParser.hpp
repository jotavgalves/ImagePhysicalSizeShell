#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ips {

enum class ImageContainerFormat {
  Unknown,
  Png,
  Jpeg,
  Tiff,
  Svg,
  Pdf
};

struct EmbeddedDpi {
  bool found = false;
  double dpiX = 0.0;
  double dpiY = 0.0;
  std::wstring source;
  std::wstring status;
};

ImageContainerFormat DetectContainerFormat(const std::vector<std::uint8_t>& bytes);
std::wstring FormatName(ImageContainerFormat format);
EmbeddedDpi DetectEmbeddedDpi(const std::vector<std::uint8_t>& bytes);

}
