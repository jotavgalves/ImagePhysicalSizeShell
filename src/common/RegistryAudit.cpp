#include "common/RegistryAudit.hpp"

#include "common/JsonWriter.hpp"
#include "common/Utf.hpp"

#include <windows.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace ips {
namespace {

constexpr wchar_t kPropertyHandlerRoot[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers";
constexpr wchar_t kShellPropertyHandlerClsid[] = L"{e357fccd-a995-4576-b01f-234630154e96}";

struct RootKey {
  HKEY key;
  std::wstring hive;
};

std::wstring JoinPath(const std::wstring& left, const std::wstring& right) {
  if (left.empty()) {
    return right;
  }
  if (right.empty()) {
    return left;
  }
  return left + L"\\" + right;
}

std::wstring TypeName(DWORD type) {
  switch (type) {
    case REG_NONE:
      return L"REG_NONE";
    case REG_SZ:
      return L"REG_SZ";
    case REG_EXPAND_SZ:
      return L"REG_EXPAND_SZ";
    case REG_BINARY:
      return L"REG_BINARY";
    case REG_DWORD:
      return L"REG_DWORD";
    case REG_MULTI_SZ:
      return L"REG_MULTI_SZ";
    case REG_QWORD:
      return L"REG_QWORD";
    default:
      return L"REG_TYPE_" + std::to_wstring(type);
  }
}

std::wstring ReadDefaultString(const RegistryValueSnapshot& value) {
  if (!value.exists) {
    return L"";
  }
  if (value.type == REG_SZ || value.type == REG_EXPAND_SZ) {
    return value.stringValue;
  }
  return L"";
}

std::vector<std::wstring> ParseMultiSz(const std::vector<unsigned char>& bytes) {
  std::vector<std::wstring> result;
  if (bytes.empty()) {
    return result;
  }

  const auto* chars = reinterpret_cast<const wchar_t*>(bytes.data());
  const size_t count = bytes.size() / sizeof(wchar_t);
  size_t start = 0;
  for (size_t i = 0; i < count; ++i) {
    if (chars[i] == L'\0') {
      if (i == start) {
        break;
      }
      result.emplace_back(chars + start, chars + i);
      start = i + 1;
    }
  }
  return result;
}

RegistryValueSnapshot QueryValue(const RootKey& root, const std::wstring& path, const std::wstring& name) {
  RegistryValueSnapshot snapshot;
  snapshot.hive = root.hive;
  snapshot.path = path;
  snapshot.name = name;

  HKEY key = nullptr;
  LSTATUS status = RegOpenKeyExW(root.key, path.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key);
  if (status != ERROR_SUCCESS) {
    status = RegOpenKeyExW(root.key, path.c_str(), 0, KEY_READ, &key);
  }
  if (status != ERROR_SUCCESS) {
    return snapshot;
  }

  DWORD type = REG_NONE;
  DWORD size = 0;
  const wchar_t* valueName = name.empty() ? nullptr : name.c_str();
  status = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &size);
  if (status != ERROR_SUCCESS) {
    RegCloseKey(key);
    return snapshot;
  }

  snapshot.exists = true;
  snapshot.type = type;
  if (size > 0) {
    snapshot.binaryValue.resize(size);
    status = RegQueryValueExW(key, valueName, nullptr, &type, snapshot.binaryValue.data(), &size);
    if (status != ERROR_SUCCESS) {
      snapshot.exists = false;
      snapshot.binaryValue.clear();
      RegCloseKey(key);
      return snapshot;
    }
    snapshot.binaryValue.resize(size);
  }

  if ((type == REG_SZ || type == REG_EXPAND_SZ) && !snapshot.binaryValue.empty()) {
    const auto* chars = reinterpret_cast<const wchar_t*>(snapshot.binaryValue.data());
    size_t charCount = snapshot.binaryValue.size() / sizeof(wchar_t);
    while (charCount > 0 && chars[charCount - 1] == L'\0') {
      --charCount;
    }
    snapshot.stringValue.assign(chars, chars + charCount);
  } else if (type == REG_MULTI_SZ) {
    snapshot.multiStringValue = ParseMultiSz(snapshot.binaryValue);
  }

  RegCloseKey(key);
  return snapshot;
}

void AppendRegistryValueJson(std::ostream& out, const RegistryValueSnapshot& value) {
  JsonObjectWriter obj(out);
  obj.StringWide("hive", value.hive);
  obj.StringWide("path", value.path);
  obj.StringWide("name", value.name.empty() ? L"(Default)" : value.name);
  obj.Bool("exists", value.exists);
  obj.Number("type", value.type);
  obj.StringWide("typeName", TypeName(value.type));

  if (!value.exists) {
    obj.Raw("data", "null");
    return;
  }

  if (value.type == REG_SZ || value.type == REG_EXPAND_SZ) {
    obj.StringWide("data", value.stringValue);
  } else if (value.type == REG_MULTI_SZ) {
    std::ostringstream arr;
    arr << "[";
    for (size_t i = 0; i < value.multiStringValue.size(); ++i) {
      if (i > 0) {
        arr << ",";
      }
      arr << "\"" << JsonEscapeWide(value.multiStringValue[i]) << "\"";
    }
    arr << "]";
    obj.Raw("data", arr.str());
  } else {
    std::ostringstream hex;
    hex << "\"";
    for (const unsigned char b : value.binaryValue) {
      hex << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    hex << "\"";
    obj.Raw("dataHex", hex.str());
  }
}

void AppendRegistryValueArray(std::ostream& out, const std::vector<RegistryValueSnapshot>& values) {
  out << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    AppendRegistryValueJson(out, values[i]);
  }
  out << "]";
}

std::vector<RegistryValueSnapshot> QueryPropertyLists(HKEY root, const std::wstring& hive,
                                                      const std::wstring& basePath) {
  std::vector<RegistryValueSnapshot> result;
  const RootKey rootKey{root, hive};
  for (const auto& name : PropertyListNames()) {
    result.push_back(QueryValue(rootKey, basePath, name));
  }
  return result;
}

ExtensionAudit CollectOne(const std::wstring& extension) {
  ExtensionAudit audit;
  audit.extension = extension;

  const RootKey hklm{HKEY_LOCAL_MACHINE, L"HKLM"};
  const RootKey hkcu{HKEY_CURRENT_USER, L"HKCU"};
  const RootKey hkcr{HKEY_CLASSES_ROOT, L"HKCR"};

  audit.hklmPropertyHandler = QueryValue(hklm, JoinPath(kPropertyHandlerRoot, extension), L"");
  audit.hkcuPropertyHandler = QueryValue(hkcu, JoinPath(kPropertyHandlerRoot, extension), L"");
  audit.hkcrDefaultProgId = QueryValue(hkcr, extension, L"");
  audit.hkcrContentType = QueryValue(hkcr, extension, L"Content Type");
  audit.hkcrPerceivedType = QueryValue(hkcr, extension, L"PerceivedType");
  audit.hkcrShellExPropertyHandler =
      QueryValue(hkcr, JoinPath(JoinPath(extension, L"ShellEx"), kShellPropertyHandlerClsid), L"");

  audit.extensionPropertyLists =
      QueryPropertyLists(HKEY_CLASSES_ROOT, L"HKCR", JoinPath(L"SystemFileAssociations", extension));
  audit.imagePropertyLists =
      QueryPropertyLists(HKEY_CLASSES_ROOT, L"HKCR", L"SystemFileAssociations\\image");

  const std::wstring progId = ReadDefaultString(audit.hkcrDefaultProgId);
  if (!progId.empty()) {
    audit.progIdPropertyLists = QueryPropertyLists(HKEY_CLASSES_ROOT, L"HKCR", progId);
  }

  return audit;
}

}

std::vector<std::wstring> SupportedExtensions() {
  return {L".png", L".jpg", L".jpeg", L".tif", L".tiff"};
}

std::vector<std::wstring> PropertyListNames() {
  return {L"FullDetails", L"PreviewDetails", L"PreviewTitle",
          L"InfoTip", L"TileInfo", L"ExtendedTileInfo"};
}

std::vector<ExtensionAudit> CollectRegistryAudit() {
  std::vector<ExtensionAudit> audits;
  for (const auto& extension : SupportedExtensions()) {
    audits.push_back(CollectOne(extension));
  }
  return audits;
}

std::string RegistryAuditToJson(const std::vector<ExtensionAudit>& audits) {
  std::ostringstream out;
  out << "{";
  out << "\"schemaVersion\":1,";
  out << "\"phase\":\"safe-readonly\",";
  out << "\"supportedExtensions\":[";
  for (size_t i = 0; i < audits.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    out << "\"" << JsonEscapeWide(audits[i].extension) << "\"";
  }
  out << "],";
  out << "\"extensions\":[";
  for (size_t i = 0; i < audits.size(); ++i) {
    if (i > 0) {
      out << ",";
    }

    const auto& audit = audits[i];
    JsonObjectWriter obj(out);
    obj.StringWide("extension", audit.extension);
    obj.Raw("hklmPropertyHandler", [&] {
      std::ostringstream s;
      AppendRegistryValueJson(s, audit.hklmPropertyHandler);
      return s.str();
    }());
    obj.Raw("hkcuPropertyHandler", [&] {
      std::ostringstream s;
      AppendRegistryValueJson(s, audit.hkcuPropertyHandler);
      return s.str();
    }());
    obj.Raw("hkcrDefaultProgId", [&] {
      std::ostringstream s;
      AppendRegistryValueJson(s, audit.hkcrDefaultProgId);
      return s.str();
    }());
    obj.Raw("hkcrContentType", [&] {
      std::ostringstream s;
      AppendRegistryValueJson(s, audit.hkcrContentType);
      return s.str();
    }());
    obj.Raw("hkcrPerceivedType", [&] {
      std::ostringstream s;
      AppendRegistryValueJson(s, audit.hkcrPerceivedType);
      return s.str();
    }());
    obj.Raw("hkcrShellExPropertyHandler", [&] {
      std::ostringstream s;
      AppendRegistryValueJson(s, audit.hkcrShellExPropertyHandler);
      return s.str();
    }());
    obj.Raw("extensionPropertyLists", [&] {
      std::ostringstream s;
      AppendRegistryValueArray(s, audit.extensionPropertyLists);
      return s.str();
    }());
    obj.Raw("progIdPropertyLists", [&] {
      std::ostringstream s;
      AppendRegistryValueArray(s, audit.progIdPropertyLists);
      return s.str();
    }());
    obj.Raw("imagePropertyLists", [&] {
      std::ostringstream s;
      AppendRegistryValueArray(s, audit.imagePropertyLists);
      return s.str();
    }());
  }
  out << "]}";
  return out.str();
}

void PrintHandlersTable(const std::vector<ExtensionAudit>& audits) {
  std::wcout << L"Extension  HKLM Property Handler CLSID                         HKCU Override  ProgID\n";
  std::wcout << L"---------  --------------------------------------------------  ------------  ------------------------------\n";
  for (const auto& audit : audits) {
    const std::wstring hklm = audit.hklmPropertyHandler.exists ? audit.hklmPropertyHandler.stringValue : L"(missing)";
    const std::wstring hkcu = audit.hkcuPropertyHandler.exists ? audit.hkcuPropertyHandler.stringValue : L"(none)";
    const std::wstring progId = audit.hkcrDefaultProgId.exists ? audit.hkcrDefaultProgId.stringValue : L"(missing)";

    std::wcout << std::left << std::setw(10) << audit.extension << L" "
               << std::left << std::setw(51) << hklm << L" "
               << std::left << std::setw(13) << hkcu << L" "
               << progId << L"\n";
  }
}

bool WriteRegistryAuditBackup(const std::filesystem::path& outDir, std::string& error) {
  try {
    std::filesystem::create_directories(outDir);
    const auto audits = CollectRegistryAudit();
    const auto json = RegistryAuditToJson(audits);
    if (!WriteUtf8File(outDir / "readonly-registry-audit.json", json, error)) {
      return false;
    }

    std::ostringstream manifest;
    manifest << "{";
    manifest << "\"schemaVersion\":1,";
    manifest << "\"phase\":\"safe-readonly\",";
    manifest << "\"note\":\"Read-only audit backup. No registry values were modified.\",";
    manifest << "\"files\":[\"readonly-registry-audit.json\"]";
    manifest << "}";
    return WriteUtf8File(outDir / "readonly-backup-manifest.json", manifest.str(), error);
  } catch (const std::exception& ex) {
    error = ex.what();
    return false;
  }
}

}
