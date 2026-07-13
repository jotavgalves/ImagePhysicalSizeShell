#pragma once

#include <propidl.h>

#include <string>

namespace ips::handler {

HRESULT InitPropVariantFromDoubleValue(double value, PROPVARIANT* propvar) noexcept;
HRESULT InitPropVariantFromStringValue(const std::wstring& value, PROPVARIANT* propvar) noexcept;
HRESULT InitPropVariantEmpty(PROPVARIANT* propvar) noexcept;

}

