#pragma once

#include "imaging/MetadataParser.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ips {

struct ImageInspection {
  std::filesystem::path path;
  ImageContainerFormat format = ImageContainerFormat::Unknown;
  unsigned int widthPixels = 0;
  unsigned int heightPixels = 0;
  double wicDpiX = 0.0;
  double wicDpiY = 0.0;
  EmbeddedDpi embeddedDpi;
  bool physicalSizeFromDocument = false;
  bool hasPhysicalSize = false;
  double widthCm = 0.0;
  double heightCm = 0.0;
  std::vector<std::wstring> notes;
};

bool InspectImageFile(const std::filesystem::path& path, ImageInspection& inspection, std::wstring& error);
void PrintInspection(const ImageInspection& inspection);

}
