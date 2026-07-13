#include "common/JsonWriter.hpp"

#include "common/Utf.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace ips {

std::string JsonEscape(std::string_view value) {
  std::ostringstream out;
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\b':
        out << "\\b";
        break;
      case '\f':
        out << "\\f";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (ch < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch);
        } else {
          out << ch;
        }
        break;
    }
  }
  return out.str();
}

std::string JsonEscapeWide(const std::wstring& value) {
  return JsonEscape(WideToUtf8(value));
}

bool WriteUtf8File(const std::filesystem::path& path, const std::string& content, std::string& error) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    error = "failed to open output file";
    return false;
  }

  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!file) {
    error = "failed to write output file";
    return false;
  }

  return true;
}

JsonObjectWriter::JsonObjectWriter(std::ostream& out) : out_(out) {
  out_ << "{";
}

JsonObjectWriter::~JsonObjectWriter() {
  out_ << "}";
}

void JsonObjectWriter::Prefix(std::string_view name) {
  if (!first_) {
    out_ << ",";
  }
  first_ = false;
  out_ << "\"" << JsonEscape(name) << "\":";
}

void JsonObjectWriter::String(std::string_view name, std::string_view value) {
  Prefix(name);
  out_ << "\"" << JsonEscape(value) << "\"";
}

void JsonObjectWriter::StringWide(std::string_view name, const std::wstring& value) {
  Prefix(name);
  out_ << "\"" << JsonEscapeWide(value) << "\"";
}

void JsonObjectWriter::Bool(std::string_view name, bool value) {
  Prefix(name);
  out_ << (value ? "true" : "false");
}

void JsonObjectWriter::Number(std::string_view name, unsigned long value) {
  Prefix(name);
  out_ << value;
}

void JsonObjectWriter::Raw(std::string_view name, std::string_view value) {
  Prefix(name);
  out_ << value;
}

}

