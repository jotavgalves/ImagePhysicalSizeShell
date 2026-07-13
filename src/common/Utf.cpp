#include "common/Utf.hpp"

#include <windows.h>

#include <stdexcept>

namespace ips {

std::string WideToUtf8(const std::wstring& value) {
  if (value.empty()) {
    return {};
  }

  const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (required <= 0) {
    throw std::runtime_error("WideCharToMultiByte failed");
  }

  std::string result(static_cast<size_t>(required), '\0');
  const int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);
  if (written != required) {
    throw std::runtime_error("WideCharToMultiByte wrote unexpected length");
  }

  return result;
}

std::wstring Utf8ToWide(const std::string& value) {
  if (value.empty()) {
    return {};
  }

  const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
  if (required <= 0) {
    throw std::runtime_error("MultiByteToWideChar failed");
  }

  std::wstring result(static_cast<size_t>(required), L'\0');
  const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), result.data(), required);
  if (written != required) {
    throw std::runtime_error("MultiByteToWideChar wrote unexpected length");
  }

  return result;
}

}

