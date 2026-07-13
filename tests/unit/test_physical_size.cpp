#include "imaging/MetadataParser.hpp"
#include "imaging/ImageInspection.hpp"
#include "imaging/PhysicalSize.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* name) {
  if (!condition) {
    std::cerr << "FAIL: " << name << "\n";
    ++g_failures;
  }
}

void CheckNear(double actual, double expected, double tolerance, const char* name) {
  Check(std::fabs(actual - expected) <= tolerance, name);
}

void AppendBe16(std::vector<std::uint8_t>& v, std::uint16_t value) {
  v.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
  v.push_back(static_cast<std::uint8_t>(value & 0xff));
}

void AppendBe32(std::vector<std::uint8_t>& v, std::uint32_t value) {
  v.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
  v.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
  v.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
  v.push_back(static_cast<std::uint8_t>(value & 0xff));
}

void AppendLe16(std::vector<std::uint8_t>& v, std::uint16_t value) {
  v.push_back(static_cast<std::uint8_t>(value & 0xff));
  v.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
}

void AppendLe32(std::vector<std::uint8_t>& v, std::uint32_t value) {
  v.push_back(static_cast<std::uint8_t>(value & 0xff));
  v.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
  v.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
  v.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
}

std::vector<std::uint8_t> PngWithPhys(std::uint32_t xPpm, std::uint32_t yPpm, std::uint8_t unit) {
  std::vector<std::uint8_t> v = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  AppendBe32(v, 9);
  v.insert(v.end(), {'p', 'H', 'Y', 's'});
  AppendBe32(v, xPpm);
  AppendBe32(v, yPpm);
  v.push_back(unit);
  AppendBe32(v, 0);
  AppendBe32(v, 0);
  v.insert(v.end(), {'I', 'D', 'A', 'T'});
  AppendBe32(v, 0);
  return v;
}

std::vector<std::uint8_t> JpegJfif(std::uint8_t unit, std::uint16_t xDensity, std::uint16_t yDensity) {
  std::vector<std::uint8_t> v = {0xff, 0xd8, 0xff, 0xe0};
  AppendBe16(v, 16);
  v.insert(v.end(), {'J', 'F', 'I', 'F', 0});
  v.push_back(1);
  v.push_back(2);
  v.push_back(unit);
  AppendBe16(v, xDensity);
  AppendBe16(v, yDensity);
  v.push_back(0);
  v.push_back(0);
  v.insert(v.end(), {0xff, 0xda, 0x00, 0x02});
  return v;
}

std::vector<std::uint8_t> TiffLe(std::uint32_t xNum,
                                 std::uint32_t xDen,
                                 std::uint32_t yNum,
                                 std::uint32_t yDen,
                                 std::uint16_t unit) {
  std::vector<std::uint8_t> v;
  v.insert(v.end(), {'I', 'I'});
  AppendLe16(v, 42);
  AppendLe32(v, 8);
  AppendLe16(v, 3);

  AppendLe16(v, 282);
  AppendLe16(v, 5);
  AppendLe32(v, 1);
  AppendLe32(v, 50);

  AppendLe16(v, 283);
  AppendLe16(v, 5);
  AppendLe32(v, 1);
  AppendLe32(v, 58);

  AppendLe16(v, 296);
  AppendLe16(v, 3);
  AppendLe32(v, 1);
  AppendLe16(v, unit);
  AppendLe16(v, 0);

  AppendLe32(v, 0);

  while (v.size() < 50) {
    v.push_back(0);
  }
  AppendLe32(v, xNum);
  AppendLe32(v, xDen);
  AppendLe32(v, yNum);
  AppendLe32(v, yDen);
  return v;
}

std::vector<std::uint8_t> JpegExifTiff(std::uint32_t xNum,
                                       std::uint32_t xDen,
                                       std::uint32_t yNum,
                                       std::uint32_t yDen,
                                       std::uint16_t unit) {
  auto tiff = TiffLe(xNum, xDen, yNum, yDen, unit);
  std::vector<std::uint8_t> v = {0xff, 0xd8, 0xff, 0xe1};
  AppendBe16(v, static_cast<std::uint16_t>(tiff.size() + 8));
  v.insert(v.end(), {'E', 'x', 'i', 'f', 0, 0});
  v.insert(v.end(), tiff.begin(), tiff.end());
  v.insert(v.end(), {0xff, 0xda, 0x00, 0x02});
  return v;
}

void TestCalculations() {
  auto size300 = ips::CalculatePhysicalSizeCm(5016, 5016, 300.0, 300.0);
  Check(size300.has_value(), "5016 300 dpi has size");
  CheckNear(size300->widthCm, 42.4688, 0.0002, "5016 300 dpi width");
  CheckNear(size300->heightCm, 42.4688, 0.0002, "5016 300 dpi height");

  auto size96 = ips::CalculatePhysicalSizeCm(5016, 5016, 96.0, 96.0);
  Check(size96.has_value(), "5016 96 dpi has size");
  CheckNear(size96->widthCm, 132.715, 0.001, "5016 96 dpi width");

  auto asym = ips::CalculatePhysicalSizeCm(5016, 5016, 300.0, 150.0);
  Check(asym.has_value(), "asymmetric dpi has size");
  CheckNear(asym->widthCm, 42.4688, 0.0002, "asymmetric width");
  CheckNear(asym->heightCm, 84.9376, 0.0002, "asymmetric height");

  Check(!ips::CalculatePhysicalSizeCm(5016, 5016, 0.0, 300.0).has_value(), "zero dpi rejected");
  Check(!ips::CalculatePhysicalSizeCm(5016, 5016, -1.0, 300.0).has_value(), "negative dpi rejected");
  Check(!ips::CalculatePhysicalSizeCm(5016, 5016, std::numeric_limits<double>::quiet_NaN(), 300.0).has_value(),
        "nan dpi rejected");
  Check(!ips::CalculatePhysicalSizeCm(5016, 5016, std::numeric_limits<double>::infinity(), 300.0).has_value(),
        "infinite dpi rejected");

  CheckNear(ips::RoundToDecimalPlaces(42.4688849378, 2), 42.47, 0.0001, "round physical cm to two places");
  CheckNear(ips::RoundToDecimalPlaces(163.000266666, 2), 163.0, 0.0001, "round near integer physical cm");

  const auto formatted = ips::FormatCentimeters(163.000266666, ips::NumberFormatOptions{2, true});
  Check(formatted == L"163", "trim trailing zeros from rounded centimeters");

  const auto formattedFraction = ips::FormatCentimeters(42.4688849378, ips::NumberFormatOptions{2, true});
  Check(formattedFraction == L"42,47" || formattedFraction == L"42.47", "format fractional centimeters with two places");
}

void TestMetadata() {
  auto png = ips::DetectEmbeddedDpi(PngWithPhys(11811, 11811, 1));
  Check(png.found, "png phys found");
  CheckNear(png.dpiX, 299.9994, 0.001, "png dpi x");

  auto pngNoUnit = ips::DetectEmbeddedDpi(PngWithPhys(11811, 11811, 0));
  Check(!pngNoUnit.found, "png pHYs without meter unit rejected");

  auto jfifInch = ips::DetectEmbeddedDpi(JpegJfif(1, 300, 150));
  Check(jfifInch.found, "jfif inch found");
  CheckNear(jfifInch.dpiX, 300.0, 0.001, "jfif inch x");
  CheckNear(jfifInch.dpiY, 150.0, 0.001, "jfif inch y");

  auto jfifCm = ips::DetectEmbeddedDpi(JpegJfif(2, 118, 118));
  Check(jfifCm.found, "jfif cm found");
  CheckNear(jfifCm.dpiX, 299.72, 0.001, "jfif cm x");

  auto jfifUnitZero = ips::DetectEmbeddedDpi(JpegJfif(0, 300, 300));
  Check(!jfifUnitZero.found, "jfif unit zero rejected");

  auto exif = ips::DetectEmbeddedDpi(JpegExifTiff(300, 1, 150, 1, 2));
  Check(exif.found, "jpeg exif rational found");
  CheckNear(exif.dpiX, 300.0, 0.001, "jpeg exif x");
  CheckNear(exif.dpiY, 150.0, 0.001, "jpeg exif y");

  auto tiffInch = ips::DetectEmbeddedDpi(TiffLe(300, 1, 300, 1, 2));
  Check(tiffInch.found, "tiff inch found");
  CheckNear(tiffInch.dpiX, 300.0, 0.001, "tiff inch x");

  auto tiffCm = ips::DetectEmbeddedDpi(TiffLe(118, 1, 118, 1, 3));
  Check(tiffCm.found, "tiff cm found");
  CheckNear(tiffCm.dpiX, 299.72, 0.001, "tiff cm x");

  std::vector<std::uint8_t> corrupt = {0xff, 0xd8, 0xff};
  Check(!ips::DetectEmbeddedDpi(corrupt).found, "corrupt jpeg rejected");
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
}

void TestDocumentMeasurements() {
  const auto temp = std::filesystem::temp_directory_path();

  const auto svgPath = temp / "ips-test-size.svg";
  WriteTextFile(svgPath, R"(<svg xmlns="http://www.w3.org/2000/svg" width="10cm" height="5cm" viewBox="0 0 100 50"></svg>)");
  ips::ImageInspection svgInspection;
  std::wstring svgError;
  Check(ips::InspectImageFile(svgPath, svgInspection, svgError), "svg inspect succeeds");
  Check(svgInspection.hasPhysicalSize, "svg has physical size");
  CheckNear(svgInspection.widthCm, 10.0, 0.001, "svg width cm");
  CheckNear(svgInspection.heightCm, 5.0, 0.001, "svg height cm");
  Check(svgInspection.embeddedDpi.status == L"Medida encontrada", "svg status portuguese");

  const auto pdfPath = temp / "ips-test-a4.pdf";
  WriteTextFile(pdfPath, "%PDF-1.4\n1 0 obj << /Type /Page /MediaBox [0 0 595.2756 841.8898] >> endobj\n%%EOF\n");
  ips::ImageInspection pdfInspection;
  std::wstring pdfError;
  Check(ips::InspectImageFile(pdfPath, pdfInspection, pdfError), "pdf inspect succeeds");
  Check(pdfInspection.hasPhysicalSize, "pdf has physical size");
  CheckNear(pdfInspection.widthCm, 21.0, 0.02, "pdf a4 width cm");
  CheckNear(pdfInspection.heightCm, 29.7, 0.02, "pdf a4 height cm");
  Check(pdfInspection.embeddedDpi.status == L"Página 1", "pdf status portuguese");

  std::filesystem::remove(svgPath);
  std::filesystem::remove(pdfPath);
}

}

int main() {
  TestCalculations();
  TestMetadata();
  TestDocumentMeasurements();

  if (g_failures != 0) {
    std::cerr << g_failures << " test(s) failed\n";
    return 1;
  }

  std::cout << "All unit tests passed\n";
  return 0;
}
