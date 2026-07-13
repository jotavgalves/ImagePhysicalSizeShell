#pragma once

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ips {

struct RegistryValueSnapshot {
  std::wstring hive;
  std::wstring path;
  std::wstring name;
  bool exists = false;
  DWORD type = REG_NONE;
  std::wstring stringValue;
  std::vector<std::wstring> multiStringValue;
  std::vector<unsigned char> binaryValue;
};

struct ExtensionAudit {
  std::wstring extension;
  RegistryValueSnapshot hklmPropertyHandler;
  RegistryValueSnapshot hkcuPropertyHandler;
  RegistryValueSnapshot hkcrDefaultProgId;
  RegistryValueSnapshot hkcrContentType;
  RegistryValueSnapshot hkcrPerceivedType;
  RegistryValueSnapshot hkcrShellExPropertyHandler;
  std::vector<RegistryValueSnapshot> extensionPropertyLists;
  std::vector<RegistryValueSnapshot> progIdPropertyLists;
  std::vector<RegistryValueSnapshot> imagePropertyLists;
};

std::vector<std::wstring> SupportedExtensions();
std::vector<std::wstring> PropertyListNames();
std::vector<ExtensionAudit> CollectRegistryAudit();
std::string RegistryAuditToJson(const std::vector<ExtensionAudit>& audits);
void PrintHandlersTable(const std::vector<ExtensionAudit>& audits);
bool WriteRegistryAuditBackup(const std::filesystem::path& outDir, std::string& error);

}

