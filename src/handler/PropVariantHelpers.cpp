#include "handler/PropVariantHelpers.hpp"

#include <windows.h>

#include <cstring>

namespace ips::handler {

HRESULT InitPropVariantFromDoubleValue(double value, PROPVARIANT* propvar) noexcept {
  if (!propvar) {
    return E_POINTER;
  }
  PropVariantInit(propvar);
  propvar->vt = VT_R8;
  propvar->dblVal = value;
  return S_OK;
}

HRESULT InitPropVariantFromStringValue(const std::wstring& value, PROPVARIANT* propvar) noexcept {
  if (!propvar) {
    return E_POINTER;
  }
  PropVariantInit(propvar);
  const size_t bytes = (value.size() + 1) * sizeof(wchar_t);
  auto* copy = static_cast<wchar_t*>(CoTaskMemAlloc(bytes));
  if (!copy) {
    return E_OUTOFMEMORY;
  }
  memcpy(copy, value.c_str(), bytes);
  propvar->vt = VT_LPWSTR;
  propvar->pwszVal = copy;
  return S_OK;
}

HRESULT InitPropVariantEmpty(PROPVARIANT* propvar) noexcept {
  if (!propvar) {
    return E_POINTER;
  }
  PropVariantInit(propvar);
  propvar->vt = VT_EMPTY;
  return S_OK;
}

}
