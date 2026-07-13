#include "handler/Module.hpp"

namespace ips::handler {
namespace {

HMODULE g_module = nullptr;
std::atomic<unsigned long> g_objectCount = 0;

}

HMODULE ModuleHandle() {
  return g_module;
}

void SetModuleHandle(HMODULE module) noexcept {
  g_module = module;
}

void IncrementObjectCount() noexcept {
  g_objectCount.fetch_add(1, std::memory_order_relaxed);
}

void DecrementObjectCount() noexcept {
  g_objectCount.fetch_sub(1, std::memory_order_relaxed);
}

bool CanUnloadNow() noexcept {
  return g_objectCount.load(std::memory_order_relaxed) == 0;
}

}

