#include <windows.h>
#include <propsys.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <xmllite.h>

#include <cwctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>

namespace {

using Microsoft::WRL::ComPtr;

struct ValidationResult {
  bool ok = true;
  unsigned int propertyCount = 0;
  std::wstring message;
};

std::wstring HResultText(HRESULT hr) {
  wchar_t buffer[32] = {};
  swprintf_s(buffer, L"0x%08X", static_cast<unsigned int>(hr));
  return buffer;
}

bool EqualsNoCase(const wchar_t* a, const wchar_t* b) {
  if (!a || !b) {
    return false;
  }
  while (*a && *b) {
    if (towlower(*a) != towlower(*b)) {
      return false;
    }
    ++a;
    ++b;
  }
  return *a == 0 && *b == 0;
}

bool MoveToAttribute(IXmlReader* reader, const wchar_t* name, std::wstring& value) {
  const wchar_t* raw = nullptr;
  const HRESULT moveHr = reader->MoveToAttributeByName(name, nullptr);
  if (moveHr != S_OK) {
    return false;
  }
  if (FAILED(reader->GetValue(&raw, nullptr)) || !raw) {
    return false;
  }
  value = raw;
  reader->MoveToElement();
  return true;
}

bool IsKnownType(const std::wstring& type) {
  static const std::set<std::wstring> known = {
      L"Double",
      L"String",
  };
  return known.contains(type);
}

ValidationResult ValidatePropDesc(const std::filesystem::path& path) {
  ValidationResult result;
  if (!PathFileExistsW(path.c_str())) {
    result.ok = false;
    result.message = L"File does not exist.";
    return result;
  }

  ComPtr<IStream> stream;
  HRESULT hr = SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL,
                                      FALSE, nullptr, &stream);
  if (FAILED(hr)) {
    result.ok = false;
    result.message = L"SHCreateStreamOnFileEx failed: " + HResultText(hr);
    return result;
  }

  ComPtr<IXmlReader> reader;
  hr = CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.GetAddressOf()), nullptr);
  if (FAILED(hr)) {
    result.ok = false;
    result.message = L"CreateXmlReader failed: " + HResultText(hr);
    return result;
  }

  hr = reader->SetInput(stream.Get());
  if (FAILED(hr)) {
    result.ok = false;
    result.message = L"IXmlReader::SetInput failed: " + HResultText(hr);
    return result;
  }

  bool sawSchema = false;
  bool sawPropertyList = false;
  std::set<std::wstring> names;
  std::set<unsigned long> propIds;

  XmlNodeType nodeType = XmlNodeType_None;
  while (S_OK == (hr = reader->Read(&nodeType))) {
    if (nodeType != XmlNodeType_Element) {
      continue;
    }

    const wchar_t* localName = nullptr;
    if (FAILED(reader->GetLocalName(&localName, nullptr)) || !localName) {
      result.ok = false;
      result.message = L"Element without local name.";
      return result;
    }

    if (EqualsNoCase(localName, L"schema")) {
      sawSchema = true;
      std::wstring version;
      if (!MoveToAttribute(reader.Get(), L"schemaVersion", version) || version != L"1.0") {
        result.ok = false;
        result.message = L"Root schemaVersion must be 1.0.";
        return result;
      }
    } else if (EqualsNoCase(localName, L"propertyDescriptionList")) {
      sawPropertyList = true;
    } else if (EqualsNoCase(localName, L"propertyDescription")) {
      std::wstring name;
      std::wstring formatId;
      std::wstring propIdText;
      if (!MoveToAttribute(reader.Get(), L"name", name) || name.empty()) {
        result.ok = false;
        result.message = L"propertyDescription missing name.";
        return result;
      }
      if (!MoveToAttribute(reader.Get(), L"formatID", formatId) ||
          !EqualsNoCase(formatId.c_str(), L"{7A4E8B66-6C8A-421D-9868-42F50F9312B4}")) {
        result.ok = false;
        result.message = L"propertyDescription has unexpected formatID: " + name;
        return result;
      }
      if (!MoveToAttribute(reader.Get(), L"propID", propIdText) || propIdText.empty()) {
        result.ok = false;
        result.message = L"propertyDescription missing propID: " + name;
        return result;
      }

      const unsigned long propId = wcstoul(propIdText.c_str(), nullptr, 10);
      if (propId < 2 || propId > 8) {
        result.ok = false;
        result.message = L"propertyDescription propID outside project range: " + name;
        return result;
      }
      if (!names.insert(name).second || !propIds.insert(propId).second) {
        result.ok = false;
        result.message = L"Duplicate property name or propID: " + name;
        return result;
      }
      ++result.propertyCount;
    } else if (EqualsNoCase(localName, L"typeInfo")) {
      std::wstring type;
      if (MoveToAttribute(reader.Get(), L"type", type) && !IsKnownType(type)) {
        result.ok = false;
        result.message = L"Unexpected typeInfo type: " + type;
        return result;
      }
    }
  }

  if (FAILED(hr)) {
    result.ok = false;
    result.message = L"XML parse failed: " + HResultText(hr);
    return result;
  }
  if (!sawSchema || !sawPropertyList || result.propertyCount != 7) {
    result.ok = false;
    result.message = L"Expected schema root, propertyDescriptionList and exactly 7 properties.";
    return result;
  }

  result.message = L"Schema structure looks valid.";
  return result;
}

bool HasAllowWrite(int argc, wchar_t** argv) {
  for (int i = 1; i < argc; ++i) {
    if (EqualsNoCase(argv[i], L"--allow-write")) {
      return true;
    }
  }
  return false;
}

void Usage() {
  std::wcout << L"RegisterSchema - ImagePhysicalSizeShell schema helper\n\n"
             << L"Usage:\n"
             << L"  RegisterSchema validate <schema.propdesc>\n"
             << L"  RegisterSchema register <schema.propdesc> --allow-write\n"
             << L"  RegisterSchema unregister <schema.propdesc> --allow-write\n\n"
             << L"Register/unregister modify the machine-wide Windows Property System schema cache.\n";
}

int ValidateCommand(const std::filesystem::path& path) {
  const auto result = ValidatePropDesc(path);
  std::wcout << result.message << L"\n";
  std::wcout << L"Property count: " << result.propertyCount << L"\n";
  return result.ok ? 0 : 1;
}

int RegisterCommand(const std::filesystem::path& path) {
  const auto validation = ValidatePropDesc(path);
  if (!validation.ok) {
    std::wcerr << L"Refusing to register invalid schema: " << validation.message << L"\n";
    return 1;
  }

  HRESULT hr = PSRegisterPropertySchema(path.c_str());
  if (FAILED(hr)) {
    std::wcerr << L"PSRegisterPropertySchema failed: " << HResultText(hr) << L"\n";
    return 1;
  }
  std::wcout << L"Schema registered.\n";
  return 0;
}

int UnregisterCommand(const std::filesystem::path& path) {
  HRESULT hr = PSUnregisterPropertySchema(path.c_str());
  if (FAILED(hr)) {
    std::wcerr << L"PSUnregisterPropertySchema failed: " << HResultText(hr) << L"\n";
    return 1;
  }
  std::wcout << L"Schema unregistered.\n";
  return 0;
}

}

int wmain(int argc, wchar_t** argv) {
  const HRESULT coinitHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool didInitializeCom = SUCCEEDED(coinitHr);
  if (FAILED(coinitHr) && coinitHr != RPC_E_CHANGED_MODE) {
    std::wcerr << L"CoInitializeEx failed: " << HResultText(coinitHr) << L"\n";
    return 1;
  }

  struct ComUninitializer {
    bool active;
    ~ComUninitializer() {
      if (active) {
        CoUninitialize();
      }
    }
  } comUninitializer{didInitializeCom};

  if (argc < 3) {
    Usage();
    return 2;
  }

  const std::wstring command = argv[1];
  const std::filesystem::path path = argv[2];

  if (EqualsNoCase(command.c_str(), L"validate")) {
    return ValidateCommand(path);
  }

  if (EqualsNoCase(command.c_str(), L"register")) {
    if (!HasAllowWrite(argc, argv)) {
      std::wcerr << L"Refusing to register without --allow-write.\n";
      return 2;
    }
    return RegisterCommand(path);
  }

  if (EqualsNoCase(command.c_str(), L"unregister")) {
    if (!HasAllowWrite(argc, argv)) {
      std::wcerr << L"Refusing to unregister without --allow-write.\n";
      return 2;
    }
    return UnregisterCommand(path);
  }

  Usage();
  return 2;
}
