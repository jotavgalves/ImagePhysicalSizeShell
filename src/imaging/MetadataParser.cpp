#include "imaging/MetadataParser.hpp"

#include "imaging/PhysicalSize.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace ips {
namespace {

constexpr double kDpiPerPixelsPerMeter = 0.0254;
constexpr double kDpiPerPixelsPerCentimeter = 2.54;

std::uint16_t U16BE(const std::uint8_t* p) {
  return static_cast<std::uint16_t>((static_cast<unsigned int>(p[0]) << 8) | p[1]);
}

std::uint32_t U32BE(const std::uint8_t* p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

std::uint16_t U16(const std::uint8_t* p, bool le) {
  if (le) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<unsigned int>(p[1]) << 8));
  }
  return U16BE(p);
}

std::uint32_t U32(const std::uint8_t* p, bool le) {
  if (le) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
  }
  return U32BE(p);
}

std::optional<double> RationalAt(const std::uint8_t* tiff, size_t size, std::uint32_t offset, bool le) {
  if (offset > size || size - offset < 8) {
    return std::nullopt;
  }
  const std::uint32_t numerator = U32(tiff + offset, le);
  const std::uint32_t denominator = U32(tiff + offset + 4, le);
  if (denominator == 0) {
    return std::nullopt;
  }
  const double value = static_cast<double>(numerator) / static_cast<double>(denominator);
  if (!std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

struct TiffResolution {
  bool found = false;
  double x = 0.0;
  double y = 0.0;
  std::uint16_t unit = 0;
};

std::optional<std::uint16_t> ShortValueFromIfdEntry(const std::uint8_t* entry, bool le) {
  return U16(entry + 8, le);
}

std::optional<TiffResolution> ParseTiffResolution(const std::uint8_t* tiff, size_t size) {
  if (size < 8) {
    return std::nullopt;
  }

  const bool le = tiff[0] == 'I' && tiff[1] == 'I';
  const bool be = tiff[0] == 'M' && tiff[1] == 'M';
  if (!le && !be) {
    return std::nullopt;
  }
  if (U16(tiff + 2, le) != 42) {
    return std::nullopt;
  }

  const std::uint32_t ifdOffset = U32(tiff + 4, le);
  if (ifdOffset > size || size - ifdOffset < 2) {
    return std::nullopt;
  }

  const std::uint16_t entryCount = U16(tiff + ifdOffset, le);
  size_t entriesOffset = static_cast<size_t>(ifdOffset) + 2;
  if (entriesOffset > size || (size - entriesOffset) / 12 < entryCount) {
    return std::nullopt;
  }

  std::optional<double> xRes;
  std::optional<double> yRes;
  std::optional<std::uint16_t> unit;

  for (std::uint16_t i = 0; i < entryCount; ++i) {
    const std::uint8_t* entry = tiff + entriesOffset + (static_cast<size_t>(i) * 12);
    const std::uint16_t tag = U16(entry, le);
    const std::uint16_t type = U16(entry + 2, le);
    const std::uint32_t count = U32(entry + 4, le);
    const std::uint32_t valueOrOffset = U32(entry + 8, le);

    if ((tag == 282 || tag == 283) && type == 5 && count == 1) {
      auto value = RationalAt(tiff, size, valueOrOffset, le);
      if (tag == 282) {
        xRes = value;
      } else {
        yRes = value;
      }
    } else if (tag == 296 && type == 3 && count == 1) {
      unit = ShortValueFromIfdEntry(entry, le);
    }
  }

  if (!xRes || !yRes || !unit) {
    return std::nullopt;
  }

  return TiffResolution{true, *xRes, *yRes, *unit};
}

EmbeddedDpi DpiFromTiffResolution(const TiffResolution& resolution, const std::wstring& source) {
  double dpiX = 0.0;
  double dpiY = 0.0;
  if (resolution.unit == 2) {
    dpiX = resolution.x;
    dpiY = resolution.y;
  } else if (resolution.unit == 3) {
    dpiX = resolution.x * kDpiPerPixelsPerCentimeter;
    dpiY = resolution.y * kDpiPerPixelsPerCentimeter;
  } else {
    return EmbeddedDpi{false, 0.0, 0.0, source, L"DPI inválido"};
  }

  if (!IsReasonableDpi(dpiX) || !IsReasonableDpi(dpiY)) {
    return EmbeddedDpi{false, 0.0, 0.0, source, L"DPI inválido"};
  }

  return EmbeddedDpi{true, dpiX, dpiY, source, L"Resolução encontrada"};
}

EmbeddedDpi ParsePng(const std::vector<std::uint8_t>& bytes) {
  static constexpr std::uint8_t signature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  if (bytes.size() < sizeof(signature) || std::memcmp(bytes.data(), signature, sizeof(signature)) != 0) {
    return EmbeddedDpi{false, 0.0, 0.0, L"PNG", L"Formato inválido"};
  }

  size_t pos = sizeof(signature);
  while (pos + 12 <= bytes.size()) {
    const std::uint32_t length = U32BE(bytes.data() + pos);
    if (length > bytes.size() || pos + 12 > bytes.size() - length) {
      return EmbeddedDpi{false, 0.0, 0.0, L"PNG", L"Arquivo truncado"};
    }

    const std::uint8_t* type = bytes.data() + pos + 4;
    const std::uint8_t* payload = bytes.data() + pos + 8;

    if (std::memcmp(type, "pHYs", 4) == 0) {
      if (length < 9) {
        return EmbeddedDpi{false, 0.0, 0.0, L"PNG pHYs", L"Arquivo truncado"};
      }

      const std::uint32_t pixelsPerMeterX = U32BE(payload);
      const std::uint32_t pixelsPerMeterY = U32BE(payload + 4);
      const std::uint8_t unit = payload[8];
      if (unit != 1) {
        return EmbeddedDpi{false, 0.0, 0.0, L"PNG pHYs", L"Sem DPI físico"};
      }

      const double dpiX = static_cast<double>(pixelsPerMeterX) * kDpiPerPixelsPerMeter;
      const double dpiY = static_cast<double>(pixelsPerMeterY) * kDpiPerPixelsPerMeter;
      if (!IsReasonableDpi(dpiX) || !IsReasonableDpi(dpiY)) {
        return EmbeddedDpi{false, 0.0, 0.0, L"PNG pHYs", L"DPI inválido"};
      }

      return EmbeddedDpi{true, dpiX, dpiY, L"PNG pHYs", L"Resolução encontrada"};
    }

    if (std::memcmp(type, "IDAT", 4) == 0) {
      break;
    }

    pos += static_cast<size_t>(length) + 12;
  }

  return EmbeddedDpi{false, 0.0, 0.0, L"PNG", L"Sem DPI físico"};
}

EmbeddedDpi DpiFromJfif(std::uint8_t unit, std::uint16_t xDensity, std::uint16_t yDensity) {
  if (unit == 0) {
    return EmbeddedDpi{false, 0.0, 0.0, L"JFIF APP0", L"Sem DPI físico"};
  }
  if (xDensity == 0 || yDensity == 0) {
    return EmbeddedDpi{false, 0.0, 0.0, L"JFIF APP0", L"DPI inválido"};
  }

  double dpiX = static_cast<double>(xDensity);
  double dpiY = static_cast<double>(yDensity);
  if (unit == 2) {
    dpiX *= kDpiPerPixelsPerCentimeter;
    dpiY *= kDpiPerPixelsPerCentimeter;
  } else if (unit != 1) {
    return EmbeddedDpi{false, 0.0, 0.0, L"JFIF APP0", L"DPI inválido"};
  }

  if (!IsReasonableDpi(dpiX) || !IsReasonableDpi(dpiY)) {
    return EmbeddedDpi{false, 0.0, 0.0, L"JFIF APP0", L"DPI inválido"};
  }

  return EmbeddedDpi{true, dpiX, dpiY, L"JFIF APP0", L"Resolução encontrada"};
}

EmbeddedDpi ParseJpeg(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < 4 || bytes[0] != 0xff || bytes[1] != 0xd8) {
    return EmbeddedDpi{false, 0.0, 0.0, L"JPEG", L"Formato inválido"};
  }

  std::optional<EmbeddedDpi> jfif;
  std::optional<EmbeddedDpi> exif;
  size_t pos = 2;

  while (pos + 4 <= bytes.size()) {
    while (pos < bytes.size() && bytes[pos] == 0xff) {
      ++pos;
    }
    if (pos >= bytes.size()) {
      break;
    }

    const std::uint8_t marker = bytes[pos++];
    if (marker == 0xda || marker == 0xd9) {
      break;
    }
    if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) {
      continue;
    }
    if (pos + 2 > bytes.size()) {
      break;
    }

    const std::uint16_t segmentLength = U16BE(bytes.data() + pos);
    if (segmentLength < 2 || pos + segmentLength > bytes.size()) {
      break;
    }

    const size_t payloadOffset = pos + 2;
    const size_t payloadLength = segmentLength - 2;
    const std::uint8_t* payload = bytes.data() + payloadOffset;

    if (marker == 0xe0 && payloadLength >= 12 && std::memcmp(payload, "JFIF\0", 5) == 0) {
      jfif = DpiFromJfif(payload[7], U16BE(payload + 8), U16BE(payload + 10));
    } else if (marker == 0xe1 && payloadLength > 14 && std::memcmp(payload, "Exif\0\0", 6) == 0) {
      auto tiff = ParseTiffResolution(payload + 6, payloadLength - 6);
      if (tiff) {
        exif = DpiFromTiffResolution(*tiff, L"EXIF IFD");
      }
    }

    pos += segmentLength;
  }

  if (exif && exif->found) {
    return *exif;
  }
  if (jfif && jfif->found) {
    return *jfif;
  }
  if (exif) {
    return *exif;
  }
  if (jfif) {
    return *jfif;
  }

  return EmbeddedDpi{false, 0.0, 0.0, L"JPEG", L"Sem DPI físico"};
}

EmbeddedDpi ParseTiff(const std::vector<std::uint8_t>& bytes) {
  auto resolution = ParseTiffResolution(bytes.data(), bytes.size());
  if (!resolution) {
    return EmbeddedDpi{false, 0.0, 0.0, L"TIFF IFD", L"Sem DPI físico"};
  }
  return DpiFromTiffResolution(*resolution, L"TIFF IFD");
}

}

ImageContainerFormat DetectContainerFormat(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
    return ImageContainerFormat::Png;
  }
  if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xd8) {
    return ImageContainerFormat::Jpeg;
  }
  if (bytes.size() >= 4 &&
      ((bytes[0] == 'I' && bytes[1] == 'I' && bytes[2] == 42 && bytes[3] == 0) ||
       (bytes[0] == 'M' && bytes[1] == 'M' && bytes[2] == 0 && bytes[3] == 42))) {
    return ImageContainerFormat::Tiff;
  }
  if (bytes.size() >= 5 && bytes[0] == '%' && bytes[1] == 'P' && bytes[2] == 'D' && bytes[3] == 'F' &&
      bytes[4] == '-') {
    return ImageContainerFormat::Pdf;
  }
  const auto probeSize = std::min<size_t>(bytes.size(), 512);
  std::string probe(reinterpret_cast<const char*>(bytes.data()), probeSize);
  std::transform(probe.begin(), probe.end(), probe.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (probe.find("<svg") != std::string::npos || probe.find(":svg") != std::string::npos) {
    return ImageContainerFormat::Svg;
  }
  return ImageContainerFormat::Unknown;
}

std::wstring FormatName(ImageContainerFormat format) {
  switch (format) {
    case ImageContainerFormat::Png:
      return L"PNG";
    case ImageContainerFormat::Jpeg:
      return L"JPEG";
    case ImageContainerFormat::Tiff:
      return L"TIFF";
    case ImageContainerFormat::Svg:
      return L"SVG";
    case ImageContainerFormat::Pdf:
      return L"PDF";
    case ImageContainerFormat::Unknown:
    default:
      return L"Desconhecido";
  }
}

EmbeddedDpi DetectEmbeddedDpi(const std::vector<std::uint8_t>& bytes) {
  switch (DetectContainerFormat(bytes)) {
    case ImageContainerFormat::Png:
      return ParsePng(bytes);
    case ImageContainerFormat::Jpeg:
      return ParseJpeg(bytes);
    case ImageContainerFormat::Tiff:
      return ParseTiff(bytes);
    case ImageContainerFormat::Svg:
      return EmbeddedDpi{false, 0.0, 0.0, L"SVG", L"Não se aplica"};
    case ImageContainerFormat::Pdf:
      return EmbeddedDpi{false, 0.0, 0.0, L"PDF", L"Não se aplica"};
    case ImageContainerFormat::Unknown:
    default:
      return EmbeddedDpi{false, 0.0, 0.0, L"Desconhecido", L"Formato não suportado"};
  }
}

}
