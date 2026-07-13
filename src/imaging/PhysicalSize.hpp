#pragma once

#include <optional>
#include <string>

namespace ips {

struct NumberFormatOptions {
  unsigned int maxDecimalPlaces = 2;
  bool trimTrailingZeros = true;
};

struct PhysicalSizeCm {
  double widthCm = 0.0;
  double heightCm = 0.0;
};

bool IsReasonableDpi(double dpi);
std::optional<PhysicalSizeCm> CalculatePhysicalSizeCm(unsigned int widthPixels,
                                                      unsigned int heightPixels,
                                                      double dpiX,
                                                      double dpiY);
double RoundToDecimalPlaces(double value, unsigned int decimalPlaces);
std::wstring FormatCentimeters(double value);
std::wstring FormatCentimeters(double value, NumberFormatOptions options);
std::wstring FormatPhysicalSizeCm(double widthCm, double heightCm);
std::wstring FormatPhysicalSizeCm(double widthCm, double heightCm, NumberFormatOptions options);

}
