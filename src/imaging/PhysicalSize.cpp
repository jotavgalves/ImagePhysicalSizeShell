#include "imaging/PhysicalSize.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

namespace ips {
namespace {

constexpr double kCmPerInch = 2.54;
constexpr double kMinReasonableDpi = 1.0;
constexpr double kMaxReasonableDpi = 100000.0;

std::wstring FormatInvariantFallback(double value, unsigned int decimalPlaces) {
  wchar_t buffer[64] = {};
  swprintf_s(buffer, L"%.*f", static_cast<int>(decimalPlaces), value);
  return buffer;
}

std::wstring TrimFormattedNumber(std::wstring value, const wchar_t* decimalSep) {
  if (!decimalSep || !*decimalSep) {
    return value;
  }
  const std::wstring sep(decimalSep);
  const size_t decimalPos = value.find(sep);
  if (decimalPos == std::wstring::npos) {
    return value;
  }

  while (!value.empty() && value.back() == L'0') {
    value.pop_back();
  }
  if (value.size() >= sep.size() && value.compare(value.size() - sep.size(), sep.size(), sep) == 0) {
    value.erase(value.size() - sep.size());
  }
  return value;
}

}

bool IsReasonableDpi(double dpi) {
  return std::isfinite(dpi) && dpi >= kMinReasonableDpi && dpi <= kMaxReasonableDpi;
}

std::optional<PhysicalSizeCm> CalculatePhysicalSizeCm(unsigned int widthPixels,
                                                      unsigned int heightPixels,
                                                      double dpiX,
                                                      double dpiY) {
  if (widthPixels == 0 || heightPixels == 0 || !IsReasonableDpi(dpiX) || !IsReasonableDpi(dpiY)) {
    return std::nullopt;
  }

  const double width = static_cast<double>(widthPixels) / dpiX * kCmPerInch;
  const double height = static_cast<double>(heightPixels) / dpiY * kCmPerInch;
  if (!std::isfinite(width) || !std::isfinite(height)) {
    return std::nullopt;
  }

  return PhysicalSizeCm{width, height};
}

double RoundToDecimalPlaces(double value, unsigned int decimalPlaces) {
  if (!std::isfinite(value)) {
    return value;
  }
  const unsigned int clampedPlaces = std::min(decimalPlaces, 12u);
  const double factor = std::pow(10.0, static_cast<double>(clampedPlaces));
  return std::round(value * factor) / factor;
}

std::wstring FormatCentimeters(double value) {
  return FormatCentimeters(value, NumberFormatOptions{});
}

std::wstring FormatCentimeters(double value, NumberFormatOptions options) {
  if (!std::isfinite(value)) {
    return L"";
  }

  options.maxDecimalPlaces = std::min(options.maxDecimalPlaces, 6u);
  value = RoundToDecimalPlaces(value, options.maxDecimalPlaces);

  wchar_t invariant[64] = {};
  swprintf_s(invariant, L"%.*f", static_cast<int>(options.maxDecimalPlaces), value);

  wchar_t decimalSep[8] = {};
  wchar_t thousandSep[8] = {};
  if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, decimalSep,
                      static_cast<int>(sizeof(decimalSep) / sizeof(decimalSep[0]))) <= 0) {
    wcscpy_s(decimalSep, L".");
  }
  if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, thousandSep,
                      static_cast<int>(sizeof(thousandSep) / sizeof(thousandSep[0]))) <= 0) {
    wcscpy_s(thousandSep, L"");
  }

  NUMBERFMTW format{};
  format.NumDigits = options.maxDecimalPlaces;
  format.LeadingZero = 1;
  format.Grouping = 0;
  format.lpDecimalSep = decimalSep;
  format.lpThousandSep = thousandSep;
  format.NegativeOrder = 1;

  const int required = GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, invariant, &format, nullptr, 0);
  if (required <= 0) {
    auto fallback = FormatInvariantFallback(value, options.maxDecimalPlaces);
    return options.trimTrailingZeros ? TrimFormattedNumber(fallback, L".") : fallback;
  }

  std::wstring formatted(static_cast<size_t>(required), L'\0');
  const int written =
      GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, invariant, &format, formatted.data(), required);
  if (written <= 0) {
    auto fallback = FormatInvariantFallback(value, options.maxDecimalPlaces);
    return options.trimTrailingZeros ? TrimFormattedNumber(fallback, L".") : fallback;
  }

  if (!formatted.empty() && formatted.back() == L'\0') {
    formatted.pop_back();
  }
  return options.trimTrailingZeros ? TrimFormattedNumber(formatted, decimalSep) : formatted;
}

std::wstring FormatPhysicalSizeCm(double widthCm, double heightCm) {
  return FormatPhysicalSizeCm(widthCm, heightCm, NumberFormatOptions{});
}

std::wstring FormatPhysicalSizeCm(double widthCm, double heightCm, NumberFormatOptions options) {
  return FormatCentimeters(widthCm, options) + L" x " + FormatCentimeters(heightCm, options) + L" cm";
}

}
