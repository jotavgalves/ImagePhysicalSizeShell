#pragma once

#include <propkeydef.h>

namespace ips::schema {

inline constexpr GUID kFormatIdImagePhysicalSize = {
    0x7a4e8b66,
    0x6c8a,
    0x421d,
    {0x98, 0x68, 0x42, 0xf5, 0x0f, 0x93, 0x12, 0xb4}};

inline constexpr PROPERTYKEY PKEY_PhysicalWidthCm = {kFormatIdImagePhysicalSize, 2};
inline constexpr PROPERTYKEY PKEY_PhysicalHeightCm = {kFormatIdImagePhysicalSize, 3};
inline constexpr PROPERTYKEY PKEY_PhysicalSizeCm = {kFormatIdImagePhysicalSize, 4};
inline constexpr PROPERTYKEY PKEY_EmbeddedDpiX = {kFormatIdImagePhysicalSize, 5};
inline constexpr PROPERTYKEY PKEY_EmbeddedDpiY = {kFormatIdImagePhysicalSize, 6};
inline constexpr PROPERTYKEY PKEY_DpiSource = {kFormatIdImagePhysicalSize, 7};
inline constexpr PROPERTYKEY PKEY_DpiStatus = {kFormatIdImagePhysicalSize, 8};

inline constexpr const wchar_t* kNamePhysicalWidthCm = L"ImagePhysicalSizeShell.PhysicalWidthCm";
inline constexpr const wchar_t* kNamePhysicalHeightCm = L"ImagePhysicalSizeShell.PhysicalHeightCm";
inline constexpr const wchar_t* kNamePhysicalSizeCm = L"ImagePhysicalSizeShell.PhysicalSizeCm";
inline constexpr const wchar_t* kNameEmbeddedDpiX = L"ImagePhysicalSizeShell.EmbeddedDpiX";
inline constexpr const wchar_t* kNameEmbeddedDpiY = L"ImagePhysicalSizeShell.EmbeddedDpiY";
inline constexpr const wchar_t* kNameDpiSource = L"ImagePhysicalSizeShell.DpiSource";
inline constexpr const wchar_t* kNameDpiStatus = L"ImagePhysicalSizeShell.DpiStatus";

}

