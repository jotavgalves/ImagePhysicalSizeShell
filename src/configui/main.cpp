#include "imaging/ImageInspection.hpp"
#include "imaging/PhysicalSize.hpp"

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kSettingsPath[] = L"Software\\ImagePhysicalSizeShell\\Settings";

enum ControlId {
  IDC_SHOW_SIZE = 1001,
  IDC_SHOW_WIDTH,
  IDC_SHOW_HEIGHT,
  IDC_SHOW_DPIX,
  IDC_SHOW_DPIY,
  IDC_SHOW_SOURCE,
  IDC_SHOW_STATUS,
  IDC_FALLBACK,
  IDC_FALLBACK_DPI,
  IDC_DECIMALS,
  IDC_TRIM,
  IDC_APPLY,
  IDC_DEFAULTS,
  IDC_OPEN,
  IDC_PREVIEW,
  IDC_STATUS,
};

struct UiState {
  bool showSize = true;
  bool showWidth = false;
  bool showHeight = false;
  bool showDpiX = false;
  bool showDpiY = false;
  bool showSource = true;
  bool showStatus = true;
  bool fallbackEnabled = false;
  int fallbackDpi = 72;
  int decimals = 2;
  bool trimZeros = true;
};

struct AppState {
  HWND hwnd = nullptr;
  std::filesystem::path selectedPath;
  ips::ImageInspection inspection;
  bool hasInspection = false;
};

AppState g_app;

HWND Item(int id) {
  return GetDlgItem(g_app.hwnd, id);
}

void SetStatus(const std::wstring& text) {
  SetWindowTextW(Item(IDC_STATUS), text.c_str());
}

bool Checked(int id) {
  return Button_GetCheck(Item(id)) == BST_CHECKED;
}

void SetChecked(int id, bool checked) {
  Button_SetCheck(Item(id), checked ? BST_CHECKED : BST_UNCHECKED);
}

int GetIntText(int id, int fallback) {
  wchar_t buffer[32] = {};
  GetWindowTextW(Item(id), buffer, 32);
  wchar_t* end = nullptr;
  const long value = wcstol(buffer, &end, 10);
  return end == buffer ? fallback : static_cast<int>(value);
}

void SetIntText(int id, int value) {
  wchar_t buffer[32] = {};
  swprintf_s(buffer, L"%d", value);
  SetWindowTextW(Item(id), buffer);
}

UiState ReadUiState() {
  UiState state;
  state.showSize = Checked(IDC_SHOW_SIZE);
  state.showWidth = Checked(IDC_SHOW_WIDTH);
  state.showHeight = Checked(IDC_SHOW_HEIGHT);
  state.showDpiX = Checked(IDC_SHOW_DPIX);
  state.showDpiY = Checked(IDC_SHOW_DPIY);
  state.showSource = Checked(IDC_SHOW_SOURCE);
  state.showStatus = Checked(IDC_SHOW_STATUS);
  state.fallbackEnabled = Checked(IDC_FALLBACK);
  state.fallbackDpi = GetIntText(IDC_FALLBACK_DPI, 72);
  if (state.fallbackDpi < 1 || state.fallbackDpi > 100000) {
    state.fallbackDpi = 72;
  }
  state.decimals = GetIntText(IDC_DECIMALS, 2);
  if (state.decimals < 0) {
    state.decimals = 0;
  }
  if (state.decimals > 6) {
    state.decimals = 6;
  }
  state.trimZeros = Checked(IDC_TRIM);
  return state;
}

void ApplyUiState(const UiState& state) {
  SetChecked(IDC_SHOW_SIZE, state.showSize);
  SetChecked(IDC_SHOW_WIDTH, state.showWidth);
  SetChecked(IDC_SHOW_HEIGHT, state.showHeight);
  SetChecked(IDC_SHOW_DPIX, state.showDpiX);
  SetChecked(IDC_SHOW_DPIY, state.showDpiY);
  SetChecked(IDC_SHOW_SOURCE, state.showSource);
  SetChecked(IDC_SHOW_STATUS, state.showStatus);
  SetChecked(IDC_FALLBACK, state.fallbackEnabled);
  SetIntText(IDC_FALLBACK_DPI, state.fallbackDpi);
  SetIntText(IDC_DECIMALS, state.decimals);
  SetChecked(IDC_TRIM, state.trimZeros);
}

DWORD ReadDword(HKEY key, const wchar_t* name, DWORD fallback) {
  DWORD value = fallback;
  DWORD size = sizeof(value);
  DWORD type = 0;
  if (RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, &type, &value, &size) != ERROR_SUCCESS) {
    return fallback;
  }
  return value;
}

UiState LoadSettingsForUi() {
  UiState state;
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
    return state;
  }
  state.showSize = ReadDword(key, L"ShowPhysicalSizeCm", state.showSize ? 1 : 0) != 0;
  state.showWidth = ReadDword(key, L"ShowPhysicalWidthCm", state.showWidth ? 1 : 0) != 0;
  state.showHeight = ReadDword(key, L"ShowPhysicalHeightCm", state.showHeight ? 1 : 0) != 0;
  state.showDpiX = ReadDword(key, L"ShowEmbeddedDpiX", state.showDpiX ? 1 : 0) != 0;
  state.showDpiY = ReadDword(key, L"ShowEmbeddedDpiY", state.showDpiY ? 1 : 0) != 0;
  state.showSource = ReadDword(key, L"ShowDpiSource", state.showSource ? 1 : 0) != 0;
  state.showStatus = ReadDword(key, L"ShowDpiStatus", state.showStatus ? 1 : 0) != 0;
  state.fallbackEnabled = ReadDword(key, L"FallbackDpiEnabled", 0) != 0;
  state.fallbackDpi = static_cast<int>(ReadDword(key, L"FallbackDpi", 72));
  state.decimals = static_cast<int>(ReadDword(key, L"MaxDecimalPlaces", 2));
  state.trimZeros = ReadDword(key, L"TrimTrailingZeros", 1) != 0;
  RegCloseKey(key);
  return state;
}

void WriteDword(HKEY key, const wchar_t* name, DWORD value) {
  RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

bool SaveSettings(const UiState& state, std::wstring& error) {
  HKEY key = nullptr;
  const LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsPath, 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                                         nullptr);
  if (status != ERROR_SUCCESS) {
    error = L"Falha ao abrir as configurações do usuário.";
    return false;
  }
  WriteDword(key, L"ShowPhysicalSizeCm", state.showSize ? 1 : 0);
  WriteDword(key, L"ShowPhysicalWidthCm", state.showWidth ? 1 : 0);
  WriteDword(key, L"ShowPhysicalHeightCm", state.showHeight ? 1 : 0);
  WriteDword(key, L"ShowEmbeddedDpiX", state.showDpiX ? 1 : 0);
  WriteDword(key, L"ShowEmbeddedDpiY", state.showDpiY ? 1 : 0);
  WriteDword(key, L"ShowDpiSource", state.showSource ? 1 : 0);
  WriteDword(key, L"ShowDpiStatus", state.showStatus ? 1 : 0);
  WriteDword(key, L"FallbackDpiEnabled", state.fallbackEnabled ? 1 : 0);
  WriteDword(key, L"FallbackDpi", static_cast<DWORD>(state.fallbackDpi));
  WriteDword(key, L"MaxDecimalPlaces", static_cast<DWORD>(state.decimals));
  WriteDword(key, L"TrimTrailingZeros", state.trimZeros ? 1 : 0);
  RegCloseKey(key);
  return true;
}

void DeleteSettingsKey() {
  RegDeleteTreeW(HKEY_CURRENT_USER, kSettingsPath);
}

ips::ImageInspection SampleInspection() {
  ips::ImageInspection inspection;
  inspection.format = ips::ImageContainerFormat::Jpeg;
  inspection.widthPixels = 9626;
  inspection.heightPixels = 9331;
  inspection.embeddedDpi.found = true;
  inspection.embeddedDpi.dpiX = 150.0;
  inspection.embeddedDpi.dpiY = 150.0;
  inspection.embeddedDpi.source = L"EXIF IFD";
  inspection.embeddedDpi.status = L"Resolução encontrada";
  auto size = ips::CalculatePhysicalSizeCm(inspection.widthPixels, inspection.heightPixels, 150.0, 150.0);
  if (size) {
    inspection.hasPhysicalSize = true;
    inspection.widthCm = size->widthCm;
    inspection.heightCm = size->heightCm;
  }
  return inspection;
}

void AddPreviewLine(const std::wstring& label, const std::wstring& value) {
  std::wstring line = label + L":    " + value;
  SendMessageW(Item(IDC_PREVIEW), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
}

std::wstring Number(double value, const UiState& state) {
  return ips::FormatCentimeters(value, ips::NumberFormatOptions{static_cast<unsigned int>(state.decimals),
                                                                state.trimZeros});
}

std::wstring NumberNoUnit(double value, const UiState& state) {
  return ips::FormatCentimeters(value, ips::NumberFormatOptions{static_cast<unsigned int>(state.decimals),
                                                                state.trimZeros});
}

void UpdatePreview() {
  const UiState state = ReadUiState();
  SendMessageW(Item(IDC_PREVIEW), LB_RESETCONTENT, 0, 0);

  ips::ImageInspection inspection = g_app.hasInspection ? g_app.inspection : SampleInspection();
  if (!inspection.embeddedDpi.found && state.fallbackEnabled && ips::IsReasonableDpi(state.fallbackDpi)) {
    auto size = ips::CalculatePhysicalSizeCm(inspection.widthPixels, inspection.heightPixels,
                                             static_cast<double>(state.fallbackDpi),
                                             static_cast<double>(state.fallbackDpi));
    if (size) {
      inspection.embeddedDpi.found = true;
      inspection.embeddedDpi.dpiX = static_cast<double>(state.fallbackDpi);
      inspection.embeddedDpi.dpiY = static_cast<double>(state.fallbackDpi);
      inspection.embeddedDpi.source = L"DPI inferido";
      inspection.embeddedDpi.status = L"DPI inferido";
      inspection.hasPhysicalSize = true;
      inspection.widthCm = size->widthCm;
      inspection.heightCm = size->heightCm;
    }
  }

  AddPreviewLine(L"Tipo", inspection.format == ips::ImageContainerFormat::Png ? L"Arquivo PNG" : L"Arquivo JPG");
  AddPreviewLine(L"Dimensões", std::to_wstring(inspection.widthPixels) + L" x " +
                                  std::to_wstring(inspection.heightPixels));

  if (state.showSize) {
    AddPreviewLine(L"Tamanho físico", inspection.hasPhysicalSize
                                         ? ips::FormatPhysicalSizeCm(
                                               inspection.widthCm, inspection.heightCm,
                                               ips::NumberFormatOptions{static_cast<unsigned int>(state.decimals),
                                                                        state.trimZeros})
                                         : L"DPI não incorporado");
  }
  if (state.showWidth) {
    AddPreviewLine(L"Largura física", inspection.hasPhysicalSize ? Number(inspection.widthCm, state) : L"");
  }
  if (state.showHeight) {
    AddPreviewLine(L"Altura física", inspection.hasPhysicalSize ? Number(inspection.heightCm, state) : L"");
  }
  if (state.showDpiX) {
    AddPreviewLine(L"DPI incorporado X",
                   inspection.embeddedDpi.found ? NumberNoUnit(inspection.embeddedDpi.dpiX, state) : L"");
  }
  if (state.showDpiY) {
    AddPreviewLine(L"DPI incorporado Y",
                   inspection.embeddedDpi.found ? NumberNoUnit(inspection.embeddedDpi.dpiY, state) : L"");
  }
  if (state.showSource) {
    AddPreviewLine(L"Origem da resolução", inspection.embeddedDpi.source);
  }
  if (state.showStatus) {
    AddPreviewLine(L"Status do DPI", inspection.embeddedDpi.status);
  }
}

HWND CreateCtl(const wchar_t* klass, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
  return CreateWindowExW(0, klass, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_app.hwnd,
                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
}

void SetFontRecursive(HWND hwnd, HFONT font) {
  SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  }
}

void OpenImage() {
  wchar_t fileName[MAX_PATH] = {};
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = g_app.hwnd;
  ofn.lpstrFilter = L"Imagens\0*.png;*.jpg;*.jpeg;*.tif;*.tiff\0Todos\0*.*\0";
  ofn.lpstrFile = fileName;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  if (!GetOpenFileNameW(&ofn)) {
    return;
  }

  ips::ImageInspection inspection;
  std::wstring error;
  if (!ips::InspectImageFile(fileName, inspection, error)) {
    SetStatus(L"Falha ao inspecionar imagem: " + error);
    return;
  }
  g_app.selectedPath = fileName;
  g_app.inspection = inspection;
  g_app.hasInspection = true;
  SetStatus(L"Prévia usando: " + g_app.selectedPath.filename().wstring());
  UpdatePreview();
}

bool RestartExplorer(std::wstring& message) {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    message = L"Configuração aplicada, mas não foi possível localizar o Explorer.";
    return false;
  }

  int terminated = 0;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (_wcsicmp(entry.szExeFile, L"explorer.exe") != 0) {
        continue;
      }

      HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
      if (!process) {
        continue;
      }

      if (TerminateProcess(process, 0)) {
        WaitForSingleObject(process, 3000);
        ++terminated;
      }
      CloseHandle(process);
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);

  Sleep(1200);
  HINSTANCE started = ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOWNORMAL);
  if (reinterpret_cast<INT_PTR>(started) <= 32) {
    message = L"Configuração aplicada; Explorer foi encerrado, mas não consegui reabrir automaticamente.";
    return false;
  }

  if (terminated > 0) {
    message = L"Configuração aplicada. Explorer reiniciado.";
  } else {
    message = L"Configuração aplicada. Explorer aberto.";
  }
  return true;
}

void BuildUi() {
  CreateCtl(L"STATIC", L"Campos no Explorer", 0, 18, 16, 220, 22, 0);
  CreateCtl(L"BUTTON", L"Tamanho físico", BS_AUTOCHECKBOX, 22, 48, 220, 22, IDC_SHOW_SIZE);
  CreateCtl(L"BUTTON", L"Largura física", BS_AUTOCHECKBOX, 22, 76, 220, 22, IDC_SHOW_WIDTH);
  CreateCtl(L"BUTTON", L"Altura física", BS_AUTOCHECKBOX, 22, 104, 220, 22, IDC_SHOW_HEIGHT);
  CreateCtl(L"BUTTON", L"DPI incorporado X", BS_AUTOCHECKBOX, 22, 132, 220, 22, IDC_SHOW_DPIX);
  CreateCtl(L"BUTTON", L"DPI incorporado Y", BS_AUTOCHECKBOX, 22, 160, 220, 22, IDC_SHOW_DPIY);
  CreateCtl(L"BUTTON", L"Origem da resolução", BS_AUTOCHECKBOX, 22, 188, 220, 22, IDC_SHOW_SOURCE);
  CreateCtl(L"BUTTON", L"Status do DPI", BS_AUTOCHECKBOX, 22, 216, 220, 22, IDC_SHOW_STATUS);

  CreateCtl(L"STATIC", L"Formato", 0, 18, 258, 220, 22, 0);
  CreateCtl(L"STATIC", L"Casas decimais", 0, 22, 288, 120, 22, 0);
  CreateCtl(L"EDIT", L"2", WS_BORDER | ES_NUMBER, 150, 286, 50, 24, IDC_DECIMALS);
  CreateCtl(L"BUTTON", L"Remover zeros finais", BS_AUTOCHECKBOX, 22, 318, 220, 22, IDC_TRIM);

  CreateCtl(L"STATIC", L"Sem DPI incorporado", 0, 18, 366, 220, 22, 0);
  CreateCtl(L"BUTTON", L"Inferir DPI", BS_AUTOCHECKBOX, 22, 396, 120, 22, IDC_FALLBACK);
  CreateCtl(L"EDIT", L"72", WS_BORDER | ES_NUMBER, 150, 394, 50, 24, IDC_FALLBACK_DPI);

  CreateCtl(L"BUTTON", L"Aplicar", BS_PUSHBUTTON, 22, 452, 94, 30, IDC_APPLY);
  CreateCtl(L"BUTTON", L"Padrão", BS_PUSHBUTTON, 126, 452, 94, 30, IDC_DEFAULTS);
  CreateCtl(L"BUTTON", L"Abrir imagem...", BS_PUSHBUTTON, 22, 492, 198, 30, IDC_OPEN);

  CreateCtl(L"STATIC", L"Prévia estilo painel de detalhes", 0, 280, 16, 420, 22, 0);
  CreateCtl(L"LISTBOX", L"", WS_BORDER | LBS_NOINTEGRALHEIGHT | WS_VSCROLL, 280, 48, 450, 390, IDC_PREVIEW);
  CreateCtl(L"STATIC", L"", WS_BORDER, 280, 452, 450, 70, IDC_STATUS);

  ApplyUiState(LoadSettingsForUi());
  SetStatus(L"Prévia usando amostra. Clique em Abrir imagem para testar arquivo real.");
  UpdatePreview();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      g_app.hwnd = hwnd;
      BuildUi();
      NONCLIENTMETRICSW metrics{sizeof(metrics)};
      SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
      HFONT font = CreateFontIndirectW(&metrics.lfMessageFont);
      SetFontRecursive(hwnd, font);
      return 0;
    }
    case WM_COMMAND: {
      const int id = LOWORD(wParam);
      if (id == IDC_APPLY) {
        std::wstring error;
        if (SaveSettings(ReadUiState(), error)) {
          std::wstring restartMessage;
          RestartExplorer(restartMessage);
          SetStatus(restartMessage);
        } else {
          SetStatus(error);
        }
        UpdatePreview();
        return 0;
      }
      if (id == IDC_DEFAULTS) {
        DeleteSettingsKey();
        ApplyUiState(UiState{});
        SetStatus(L"Padrão restaurado.");
        UpdatePreview();
        return 0;
      }
      if (id == IDC_OPEN) {
        OpenImage();
        return 0;
      }
      if (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == EN_CHANGE) {
        UpdatePreview();
      }
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCmd) {
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&icc);

  const wchar_t className[] = L"ImagePhysicalSizeShellConfigUi";
  WNDCLASSW wc{};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = className;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0, className, L"ImagePhysicalSizeShell - Configuração", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 770, 590, nullptr, nullptr, instance, nullptr);
  if (!hwnd) {
    return 1;
  }

  ShowWindow(hwnd, showCmd);
  UpdateWindow(hwnd);

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}
