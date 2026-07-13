#pragma once

#include <windows.h>

#include <optional>
#include <string>

namespace ips::handler {

std::optional<CLSID> ReadSavedOriginalHandlerClsid(const std::wstring& canonicalExtension);
std::wstring CanonicalExtensionForFormatName(const std::wstring& formatName);

}

