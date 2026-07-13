#include "common/RegistryAudit.hpp"
#include "common/Utf.hpp"
#include "imaging/ImageInspection.hpp"

#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <propvarutil.h>
#include <propsys.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include "property_schema/PropertyKeys.hpp"
#include "handler/ComIds.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using Microsoft::WRL::ComPtr;

struct NamedProperty {
  const wchar_t* name;
  PROPERTYKEY key;
};

constexpr NamedProperty kProjectProperties[] = {
    {ips::schema::kNamePhysicalWidthCm, ips::schema::PKEY_PhysicalWidthCm},
    {ips::schema::kNamePhysicalHeightCm, ips::schema::PKEY_PhysicalHeightCm},
    {ips::schema::kNamePhysicalSizeCm, ips::schema::PKEY_PhysicalSizeCm},
    {ips::schema::kNameEmbeddedDpiX, ips::schema::PKEY_EmbeddedDpiX},
    {ips::schema::kNameEmbeddedDpiY, ips::schema::PKEY_EmbeddedDpiY},
    {ips::schema::kNameDpiSource, ips::schema::PKEY_DpiSource},
    {ips::schema::kNameDpiStatus, ips::schema::PKEY_DpiStatus},
};

std::wstring HResultText(HRESULT hr) {
  wchar_t buffer[32] = {};
  swprintf_s(buffer, L"0x%08X", static_cast<unsigned int>(hr));
  return buffer;
}

std::wstring PropVariantToDisplayString(const PROPVARIANT& value) {
  if (value.vt == VT_EMPTY) {
    return L"(empty)";
  }
  if (value.vt == VT_R8) {
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%.12g", value.dblVal);
    return buffer;
  }

  PWSTR text = nullptr;
  if (SUCCEEDED(PropVariantToStringAlloc(value, &text)) && text) {
    std::wstring result(text);
    CoTaskMemFree(text);
    return result;
  }

  return L"(unprintable vt=" + std::to_wstring(value.vt) + L")";
}

void PrintUsage() {
  std::wcout
      << L"ipsdiag - diagnósticos do ImagePhysicalSizeShell\n\n"
      << L"Uso:\n"
      << L"  ipsdiag handlers\n"
      << L"  ipsdiag status\n"
      << L"  ipsdiag backup-registry <directory>\n"
      << L"  ipsdiag inspect <file>\n"
      << L"  ipsdiag shell <file>\n"
      << L"  ipsdiag proxy <file>\n"
      << L"  ipsdiag original <file> <clsid>\n"
      << L"  ipsdiag compare-before-after <file> (implementado em fase posterior)\n";
}

int CommandHandlers() {
  const auto audits = ips::CollectRegistryAudit();
  ips::PrintHandlersTable(audits);
  return 0;
}

int CommandStatus() {
  std::wcout << L"Status do ImagePhysicalSizeShell\n";
  std::wcout << L"Arquitetura do processo: " << (sizeof(void*) == 8 ? L"x64" : L"não x64") << L"\n";
  std::wcout << L"Arquitetura do sistema: " << (sizeof(void*) == 8 ? L"compatível com x64" : L"desconhecida")
             << L"\n";
  std::wcout << L"Operações de escrita no Registro: controladas pelo instalador\n";
  std::wcout << L"Registro COM: verificado pelo instalador\n";
  std::wcout << L"Registro do schema de propriedades: verificado pelo instalador\n\n";
  return CommandHandlers();
}

int CommandBackupRegistry(int argc, wchar_t** argv) {
  if (argc < 3) {
    std::wcerr << L"backup-registry exige uma pasta de saída.\n";
    return 2;
  }

  std::string error;
  if (!ips::WriteRegistryAuditBackup(std::filesystem::path(argv[2]), error)) {
    std::wcerr << L"Falha ao gravar auditoria somente leitura do Registro: " << ips::Utf8ToWide(error) << L"\n";
    return 1;
  }

  std::wcout << L"Auditoria somente leitura do Registro gravada em " << argv[2] << L"\n";
  return 0;
}

int NotImplementedYet(const wchar_t* command) {
  std::wcerr << L"O comando '" << command << L"' ainda está fora da fase segura atual.\n";
  return 3;
}

int CommandInspect(int argc, wchar_t** argv) {
  if (argc < 3) {
    std::wcerr << L"inspect exige o caminho de uma imagem.\n";
    return 2;
  }

  ips::ImageInspection inspection;
  std::wstring error;
  if (!ips::InspectImageFile(std::filesystem::path(argv[2]), inspection, error)) {
    std::wcerr << L"inspect falhou: " << error << L"\n";
    return 1;
  }

  ips::PrintInspection(inspection);
  return 0;
}

int CommandShell(int argc, wchar_t** argv) {
  if (argc < 3) {
    std::wcerr << L"shell exige o caminho de uma imagem.\n";
    return 2;
  }

  const std::filesystem::path path(argv[2]);
  ComPtr<IPropertyStore> store;
  const GETPROPERTYSTOREFLAGS flags =
      static_cast<GETPROPERTYSTOREFLAGS>(GPS_HANDLERPROPERTIESONLY | GPS_OPENSLOWITEM);
  HRESULT hr = SHGetPropertyStoreFromParsingName(path.c_str(), nullptr, flags, IID_PPV_ARGS(&store));
  if (FAILED(hr)) {
    std::wcerr << L"SHGetPropertyStoreFromParsingName failed: " << HResultText(hr) << L"\n";
    return 1;
  }

  DWORD count = 0;
  hr = store->GetCount(&count);
  if (FAILED(hr)) {
    std::wcerr << L"IPropertyStore::GetCount failed: " << HResultText(hr) << L"\n";
    return 1;
  }

  std::wcout << L"Arquivo: " << path.c_str() << L"\n";
  std::wcout << L"Total de propriedades: " << count << L"\n";
  std::wcout << L"Flags: GPS_HANDLERPROPERTIESONLY | GPS_OPENSLOWITEM\n";
  std::wcout << L"Propriedades do ImagePhysicalSizeShell via Windows Property System:\n";

  for (const auto& property : kProjectProperties) {
    PROPVARIANT value;
    PropVariantInit(&value);
    hr = store->GetValue(property.key, &value);
    if (FAILED(hr)) {
      std::wcout << L"  " << property.name << L": <HRESULT " << HResultText(hr) << L">\n";
      continue;
    }
    std::wcout << L"  " << property.name << L": " << PropVariantToDisplayString(value) << L"\n";
    PropVariantClear(&value);
  }

  return 0;
}

int CommandProxy(int argc, wchar_t** argv) {
  if (argc < 3) {
    std::wcerr << L"proxy exige o caminho de uma imagem.\n";
    return 2;
  }

  const std::filesystem::path path(argv[2]);
  ComPtr<IUnknown> unknown;
  HRESULT hr = CoCreateInstance(ips::handler::CLSID_ImagePhysicalSizePropertyHandler, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&unknown));
  std::wcout << L"CoCreateInstance: " << HResultText(hr) << L"\n";
  if (FAILED(hr)) {
    return 1;
  }

  ComPtr<IInitializeWithFile> fileInitializer;
  hr = unknown.As(&fileInitializer);
  std::wcout << L"QI IInitializeWithFile: " << HResultText(hr) << L"\n";
  if (FAILED(hr)) {
    return 1;
  }

  hr = fileInitializer->Initialize(path.c_str(), STGM_READ);
  std::wcout << L"IInitializeWithFile::Initialize: " << HResultText(hr) << L"\n";
  if (FAILED(hr)) {
    return 1;
  }

  ComPtr<IPropertyStore> store;
  hr = unknown.As(&store);
  std::wcout << L"QI IPropertyStore: " << HResultText(hr) << L"\n";
  if (FAILED(hr)) {
    return 1;
  }

  DWORD count = 0;
  hr = store->GetCount(&count);
  std::wcout << L"GetCount: " << HResultText(hr) << L" count=" << count << L"\n";
  if (FAILED(hr)) {
    return 1;
  }

  for (const auto& property : kProjectProperties) {
    PROPVARIANT value;
    PropVariantInit(&value);
    hr = store->GetValue(property.key, &value);
    std::wcout << L"  " << property.name << L": hr=" << HResultText(hr);
    if (SUCCEEDED(hr)) {
      std::wcout << L" value=" << PropVariantToDisplayString(value);
    }
    std::wcout << L"\n";
    PropVariantClear(&value);
  }

  return 0;
}

int CommandOriginal(int argc, wchar_t** argv) {
  if (argc < 4) {
    std::wcerr << L"original exige o caminho de uma imagem e um CLSID.\n";
    return 2;
  }

  CLSID clsid{};
  HRESULT hr = CLSIDFromString(argv[3], &clsid);
  std::wcout << L"CLSIDFromString: " << HResultText(hr) << L"\n";
  if (FAILED(hr)) {
    return 1;
  }

  const std::filesystem::path path(argv[2]);
  ComPtr<IUnknown> unknown;
  hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&unknown));
  std::wcout << L"CoCreateInstance original: " << HResultText(hr) << L"\n";
  if (FAILED(hr)) {
    return 1;
  }

  ComPtr<IPropertyStore> store;
  HRESULT storeHr = unknown.As(&store);
  std::wcout << L"QI IPropertyStore before init: " << HResultText(storeHr) << L"\n";

  ComPtr<IInitializeWithStream> streamInitializer;
  hr = unknown.As(&streamInitializer);
  std::wcout << L"QI IInitializeWithStream: " << HResultText(hr) << L"\n";
  if (SUCCEEDED(hr)) {
    ComPtr<IStream> stream;
    HRESULT streamHr = SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL,
                                              FALSE, nullptr, stream.GetAddressOf());
    std::wcout << L"SHCreateStreamOnFileEx: " << HResultText(streamHr) << L"\n";
    if (SUCCEEDED(streamHr)) {
      HRESULT initHr = streamInitializer->Initialize(stream.Get(), STGM_READ);
      std::wcout << L"IInitializeWithStream::Initialize: " << HResultText(initHr) << L"\n";
    }
  }

  ComPtr<IInitializeWithFile> fileInitializer;
  hr = unknown.As(&fileInitializer);
  std::wcout << L"QI IInitializeWithFile: " << HResultText(hr) << L"\n";
  if (SUCCEEDED(hr)) {
    HRESULT initHr = fileInitializer->Initialize(path.c_str(), STGM_READ);
    std::wcout << L"IInitializeWithFile::Initialize: " << HResultText(initHr) << L"\n";
  }

  ComPtr<IInitializeWithItem> itemInitializer;
  hr = unknown.As(&itemInitializer);
  std::wcout << L"QI IInitializeWithItem: " << HResultText(hr) << L"\n";
  if (SUCCEEDED(hr)) {
    ComPtr<IShellItem> item;
    HRESULT itemHr = SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&item));
    std::wcout << L"SHCreateItemFromParsingName: " << HResultText(itemHr) << L"\n";
    if (SUCCEEDED(itemHr)) {
      HRESULT initHr = itemInitializer->Initialize(item.Get(), STGM_READ);
      std::wcout << L"IInitializeWithItem::Initialize: " << HResultText(initHr) << L"\n";
    }
  }

  return 0;
}

}

int wmain(int argc, wchar_t** argv) {
  _setmode(_fileno(stdout), _O_U8TEXT);
  _setmode(_fileno(stderr), _O_U8TEXT);

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

  try {
    if (argc < 2) {
      PrintUsage();
      return 2;
    }

    const std::wstring command = argv[1];
    if (command == L"handlers") {
      return CommandHandlers();
    }
    if (command == L"status") {
      return CommandStatus();
    }
    if (command == L"backup-registry") {
      return CommandBackupRegistry(argc, argv);
    }
    if (command == L"inspect") {
      return CommandInspect(argc, argv);
    }
    if (command == L"shell") {
      return CommandShell(argc, argv);
    }
    if (command == L"proxy") {
      return CommandProxy(argc, argv);
    }
    if (command == L"original") {
      return CommandOriginal(argc, argv);
    }
    if (command == L"compare-before-after") {
      return NotImplementedYet(command.c_str());
    }

    PrintUsage();
    return 2;
  } catch (const std::exception& ex) {
    std::wcerr << L"Erro fatal no diagnóstico: " << ips::Utf8ToWide(ex.what()) << L"\n";
    return 1;
  } catch (...) {
    std::wcerr << L"Erro fatal no diagnóstico: exceção desconhecida\n";
    return 1;
  }
}
