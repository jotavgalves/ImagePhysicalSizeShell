#include "imaging/ImageInspection.hpp"

#include "imaging/PhysicalSize.hpp"

#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace ips {
namespace {

using Microsoft::WRL::ComPtr;

constexpr size_t kMaxMetadataScanBytes = 16u * 1024u * 1024u;
constexpr double kCmPerInch = 2.54;
constexpr double kCssPxPerInch = 96.0;
constexpr double kPdfPointsPerInch = 72.0;

class ComApartment {
 public:
  ComApartment() {
    hr_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    owns_ = SUCCEEDED(hr_);
    if (hr_ == RPC_E_CHANGED_MODE) {
      hr_ = S_OK;
      owns_ = false;
    }
  }

  ~ComApartment() {
    if (owns_) {
      CoUninitialize();
    }
  }

  HRESULT hr() const { return hr_; }

 private:
  HRESULT hr_ = E_FAIL;
  bool owns_ = false;
};

std::vector<std::uint8_t> ReadMetadataPrefix(const std::filesystem::path& path, std::wstring& error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = L"Não foi possível abrir o arquivo para ler metadados.";
    return {};
  }

  file.seekg(0, std::ios::end);
  const std::streamoff fileSize = file.tellg();
  if (fileSize < 0) {
    error = L"Não foi possível determinar o tamanho do arquivo.";
    return {};
  }
  file.seekg(0, std::ios::beg);

  const size_t bytesToRead =
      std::min(static_cast<size_t>(fileSize), static_cast<size_t>(kMaxMetadataScanBytes));
  std::vector<std::uint8_t> bytes(bytesToRead);
  if (bytesToRead > 0) {
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file && file.gcount() != static_cast<std::streamsize>(bytes.size())) {
      error = L"Não foi possível ler o início dos metadados.";
      return {};
    }
  }

  if (static_cast<size_t>(fileSize) > kMaxMetadataScanBytes) {
    error = L"A leitura de metadados foi limitada aos primeiros 16 MiB.";
  }
  return bytes;
}

std::wstring HResultText(HRESULT hr) {
  wchar_t buffer[32] = {};
  swprintf_s(buffer, L"0x%08X", static_cast<unsigned int>(hr));
  return buffer;
}

std::string BytesToText(const std::vector<std::uint8_t>& bytes) {
  return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::optional<double> ParseDouble(std::string_view text) {
  double value = 0.0;
  const auto* begin = text.data();
  const auto* end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, value);
  if (result.ec != std::errc{} || !std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

std::optional<double> SvgLengthToCm(std::string value, bool& inferred) {
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
                return !std::isspace(ch);
              }));
  value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
              value.end());
  if (value.empty() || value.find('%') != std::string::npos) {
    return std::nullopt;
  }

  std::smatch match;
  static const std::regex lengthRegex(R"(^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*([a-zA-Z]*)$)");
  if (!std::regex_match(value, match, lengthRegex)) {
    return std::nullopt;
  }

  auto number = ParseDouble(match[1].str());
  if (!number || *number <= 0.0) {
    return std::nullopt;
  }

  std::string unit = match[2].str();
  std::transform(unit.begin(), unit.end(), unit.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (unit == "cm") {
    return *number;
  }
  if (unit == "mm") {
    return *number / 10.0;
  }
  if (unit == "in") {
    return *number * kCmPerInch;
  }
  if (unit == "pt") {
    return *number / 72.0 * kCmPerInch;
  }
  if (unit == "pc") {
    return *number / 6.0 * kCmPerInch;
  }
  if (unit.empty() || unit == "px") {
    inferred = true;
    return *number / kCssPxPerInch * kCmPerInch;
  }
  return std::nullopt;
}

std::optional<std::string> SvgAttribute(const std::string& text, const char* name) {
  const std::string pattern = std::string(name) + R"(\s*=\s*["']([^"']+)["'])";
  const std::regex regex(pattern, std::regex_constants::icase);
  std::smatch match;
  if (!std::regex_search(text, match, regex)) {
    return std::nullopt;
  }
  return match[1].str();
}

bool InspectSvgDocument(const std::vector<std::uint8_t>& bytes, ImageInspection& inspection) {
  const std::string text = BytesToText(bytes);
  const auto widthText = SvgAttribute(text, "width");
  const auto heightText = SvgAttribute(text, "height");
  inspection.embeddedDpi = EmbeddedDpi{false, 0.0, 0.0, L"SVG", L"Sem medida física"};
  if (!widthText || !heightText) {
    return true;
  }

  bool inferred = false;
  auto widthCm = SvgLengthToCm(*widthText, inferred);
  auto heightCm = SvgLengthToCm(*heightText, inferred);
  if (!widthCm || !heightCm) {
    return true;
  }

  inspection.hasPhysicalSize = true;
  inspection.physicalSizeFromDocument = true;
  inspection.widthCm = *widthCm;
  inspection.heightCm = *heightCm;
  inspection.embeddedDpi.source = inferred ? L"SVG px 96 DPI" : L"SVG tamanho";
  inspection.embeddedDpi.status = inferred ? L"Medida inferida" : L"Medida encontrada";
  return true;
}

bool InspectPdfDocument(const std::vector<std::uint8_t>& bytes, ImageInspection& inspection) {
  const std::string text = BytesToText(bytes);
  inspection.embeddedDpi = EmbeddedDpi{false, 0.0, 0.0, L"PDF", L"Sem medida física"};

  static const std::regex mediaBoxRegex(
      R"(/MediaBox\s*\[\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s+([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s+([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s+([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*\])",
      std::regex_constants::icase);
  std::smatch match;
  if (!std::regex_search(text, match, mediaBoxRegex)) {
    return true;
  }

  auto x0 = ParseDouble(match[1].str());
  auto y0 = ParseDouble(match[2].str());
  auto x1 = ParseDouble(match[3].str());
  auto y1 = ParseDouble(match[4].str());
  if (!x0 || !y0 || !x1 || !y1) {
    return true;
  }

  const double widthPt = std::fabs(*x1 - *x0);
  const double heightPt = std::fabs(*y1 - *y0);
  if (widthPt <= 0.0 || heightPt <= 0.0) {
    return true;
  }

  inspection.hasPhysicalSize = true;
  inspection.physicalSizeFromDocument = true;
  inspection.widthCm = widthPt / kPdfPointsPerInch * kCmPerInch;
  inspection.heightCm = heightPt / kPdfPointsPerInch * kCmPerInch;
  inspection.embeddedDpi.source = L"PDF MediaBox";
  inspection.embeddedDpi.status = L"Página 1";
  return true;
}

}

bool InspectImageFile(const std::filesystem::path& path, ImageInspection& inspection, std::wstring& error) {
  inspection = ImageInspection{};
  inspection.path = path;

  std::wstring prefixWarning;
  const std::vector<std::uint8_t> bytes = ReadMetadataPrefix(path, prefixWarning);
  if (bytes.empty()) {
    error = prefixWarning.empty() ? L"Arquivo vazio ou ilegível." : prefixWarning;
    return false;
  }
  if (!prefixWarning.empty()) {
    inspection.notes.push_back(prefixWarning);
  }

  inspection.format = DetectContainerFormat(bytes);
  inspection.embeddedDpi = DetectEmbeddedDpi(bytes);

  if (inspection.format == ImageContainerFormat::Svg) {
    return InspectSvgDocument(bytes, inspection);
  }
  if (inspection.format == ImageContainerFormat::Pdf) {
    return InspectPdfDocument(bytes, inspection);
  }

  ComApartment com;
  if (FAILED(com.hr())) {
    error = L"CoInitializeEx failed: " + HResultText(com.hr());
    return false;
  }

  ComPtr<IWICImagingFactory> factory;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    error = L"CoCreateInstance(CLSID_WICImagingFactory) failed: " + HResultText(hr);
    return false;
  }

  ComPtr<IWICBitmapDecoder> decoder;
  hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                          &decoder);
  if (FAILED(hr)) {
    error = L"WIC CreateDecoderFromFilename failed: " + HResultText(hr);
    return false;
  }

  ComPtr<IWICBitmapFrameDecode> frame;
  hr = decoder->GetFrame(0, &frame);
  if (FAILED(hr)) {
    error = L"WIC GetFrame(0) failed: " + HResultText(hr);
    return false;
  }

  UINT width = 0;
  UINT height = 0;
  hr = frame->GetSize(&width, &height);
  if (FAILED(hr)) {
    error = L"WIC GetSize failed: " + HResultText(hr);
    return false;
  }
  inspection.widthPixels = width;
  inspection.heightPixels = height;

  double dpiX = 0.0;
  double dpiY = 0.0;
  hr = frame->GetResolution(&dpiX, &dpiY);
  if (SUCCEEDED(hr)) {
    inspection.wicDpiX = dpiX;
    inspection.wicDpiY = dpiY;
  } else {
    inspection.notes.push_back(L"WIC GetResolution failed: " + HResultText(hr));
  }

  if (inspection.embeddedDpi.found) {
    auto size = CalculatePhysicalSizeCm(width, height, inspection.embeddedDpi.dpiX, inspection.embeddedDpi.dpiY);
    if (size) {
      inspection.hasPhysicalSize = true;
      inspection.widthCm = size->widthCm;
      inspection.heightCm = size->heightCm;
    } else {
      inspection.notes.push_back(L"O cálculo do tamanho físico rejeitou o DPI incorporado ou as dimensões.");
    }
  }

  return true;
}

void PrintInspection(const ImageInspection& inspection) {
  std::wcout << L"Arquivo: " << inspection.path.wstring() << L"\n";
  std::wcout << L"Formato: " << FormatName(inspection.format) << L"\n";
  std::wcout << L"Dimensões em pixels: " << inspection.widthPixels << L" x " << inspection.heightPixels << L"\n";
  if (!inspection.physicalSizeFromDocument) {
    std::wcout << L"WIC GetResolution: " << inspection.wicDpiX << L" x " << inspection.wicDpiY << L" DPI\n";
  }
  std::wcout << L"Origem: " << inspection.embeddedDpi.source << L"\n";
  std::wcout << L"Status: " << inspection.embeddedDpi.status << L"\n";

  if (inspection.embeddedDpi.found && !inspection.physicalSizeFromDocument) {
    std::wcout << L"DPI incorporado: " << inspection.embeddedDpi.dpiX << L" x " << inspection.embeddedDpi.dpiY
               << L"\n";
  } else if (!inspection.physicalSizeFromDocument) {
    std::wcout << L"DPI incorporado: indisponível\n";
  }

  if (inspection.hasPhysicalSize) {
    std::wcout << L"Largura física: " << FormatCentimeters(inspection.widthCm) << L" cm\n";
    std::wcout << L"Altura física: " << FormatCentimeters(inspection.heightCm) << L" cm\n";
    std::wcout << L"Tamanho físico: " << FormatPhysicalSizeCm(inspection.widthCm, inspection.heightCm) << L"\n";
  } else {
    std::wcout << L"Tamanho físico: DPI não incorporado\n";
  }

  for (const auto& note : inspection.notes) {
    std::wcout << L"Observação: " << note << L"\n";
  }
}

}
