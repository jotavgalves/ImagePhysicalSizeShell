#pragma once

#include <windows.h>

#include <atomic>

namespace ips::handler {

HMODULE ModuleHandle();
void SetModuleHandle(HMODULE module) noexcept;
void IncrementObjectCount() noexcept;
void DecrementObjectCount() noexcept;
bool CanUnloadNow() noexcept;

}

