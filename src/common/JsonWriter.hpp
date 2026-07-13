#pragma once

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace ips {

std::string JsonEscape(std::string_view value);
std::string JsonEscapeWide(const std::wstring& value);
bool WriteUtf8File(const std::filesystem::path& path, const std::string& content, std::string& error);

class JsonObjectWriter {
 public:
  explicit JsonObjectWriter(std::ostream& out);
  ~JsonObjectWriter();

  void String(std::string_view name, std::string_view value);
  void StringWide(std::string_view name, const std::wstring& value);
  void Bool(std::string_view name, bool value);
  void Number(std::string_view name, unsigned long value);
  void Raw(std::string_view name, std::string_view value);

 private:
  void Prefix(std::string_view name);

  std::ostream& out_;
  bool first_ = true;
};

}

