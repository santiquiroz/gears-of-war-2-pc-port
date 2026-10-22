// gow2 - ReXGlue Recompiled Project
#include "gow2_gpu_pick.h"

#include <optional>

#include <rex/cvar.h>

#include "gow2_native_renderer_math.h"

REXCVAR_DEFINE_STRING(gow2_gpu, "auto", "UI/D3D12",
                      "GPU to render on: auto (the one Windows sets as high performance), part "
                      "of its name (for example 7800) or its adapter index. Takes effect on the "
                      "next start");

REXCVAR_DECLARE(std::string, gow2_native_renderer);

namespace rex::glue {

bool NativeRendererActive() {
  static const bool active = [] {
    const std::string mode = REXCVAR_GET(gow2_native_renderer);
    const bool path_set_by_player =
        rex::cvar::GetFlagSource("render_target_path_d3d12") != rex::cvar::Source::kDefault;
    return native_renderer::UseNativeRenderer(mode, path_set_by_player, ChooseRenderGpu().name);
  }();
  return active;
}

}  // namespace rex::glue

#ifdef _WIN32

#include <algorithm>
#include <cctype>
#include <vector>

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace rex::glue {

namespace {

struct Adapter {
  int index;
  std::string name;
  LUID luid;
  SIZE_T dedicated;
};

std::string Narrow(const wchar_t* text) {
  char out[256]{};
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out, sizeof(out) - 1, nullptr, nullptr);
  return out;
}

std::string Lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return text;
}

IDXGIFactory1* CreateFactory() {
  using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
  HMODULE dxgi = LoadLibraryA("dxgi.dll");
  auto create =
      dxgi ? reinterpret_cast<CreateFactoryFn>(GetProcAddress(dxgi, "CreateDXGIFactory1")) : nullptr;
  IDXGIFactory1* factory = nullptr;
  if (!create || FAILED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
    return nullptr;
  }
  return factory;
}

std::vector<Adapter> ListHardwareAdapters(IDXGIFactory1* factory) {
  std::vector<Adapter> adapters;
  IDXGIAdapter1* adapter = nullptr;
  for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
    DXGI_ADAPTER_DESC1 desc{};
    if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
      adapters.push_back({int(i), Narrow(desc.Description), desc.AdapterLuid,
                          desc.DedicatedVideoMemory});
    }
    adapter->Release();
  }
  return adapters;
}

// Windows' ranking: the per-app choice in Graphics settings, then the system default
// high-performance GPU, then its own guess, which ranks by dedicated memory when it does not know a
// GPU is integrated. A shared-memory reservation for an integrated GPU counts as dedicated memory,
// so a 16 GB reservation put the integrated Radeon first.
std::vector<LUID> HighPerformanceOrder(IDXGIFactory1* factory) {
  std::vector<LUID> order;
  IDXGIFactory6* factory6 = nullptr;
  if (FAILED(factory->QueryInterface(__uuidof(IDXGIFactory6), reinterpret_cast<void**>(&factory6)))) {
    return order;
  }
  IDXGIAdapter1* adapter = nullptr;
  for (UINT i = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(
           i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter1),
           reinterpret_cast<void**>(&adapter)));
       ++i) {
    DXGI_ADAPTER_DESC1 desc{};
    if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
      order.push_back(desc.AdapterLuid);
    }
    adapter->Release();
  }
  factory6->Release();
  return order;
}

// D3D12 reports UMA for integrated GPUs even when Windows no longer classifies them as integrated.
std::optional<bool> QueryUma(IDXGIFactory1* factory, LUID luid) {
  using CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
  static HMODULE d3d12 = LoadLibraryA("d3d12.dll");
  auto create = d3d12 ? reinterpret_cast<CreateDeviceFn>(GetProcAddress(d3d12, "D3D12CreateDevice"))
                      : nullptr;
  IDXGIFactory4* factory4 = nullptr;
  if (!create ||
      FAILED(factory->QueryInterface(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory4)))) {
    return std::nullopt;
  }
  IDXGIAdapter1* adapter = nullptr;
  ID3D12Device* device = nullptr;
  D3D12_FEATURE_DATA_ARCHITECTURE arch{};
  std::optional<bool> uma;
  if (SUCCEEDED(factory4->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapter1),
                                            reinterpret_cast<void**>(&adapter))) &&
      SUCCEEDED(create(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                       reinterpret_cast<void**>(&device))) &&
      SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch)))) {
    uma = arch.UMA != FALSE;
  }
  if (device) device->Release();
  if (adapter) adapter->Release();
  factory4->Release();
  return uma;
}

bool QueryIntegrated(IDXGIFactory1* factory, const Adapter& adapter) {
  const std::optional<bool> uma = QueryUma(factory, adapter.luid);
  return LooksIntegrated(uma.has_value(), uma.value_or(false), uint64_t(adapter.dedicated));
}

const Adapter* FindByLuid(const std::vector<Adapter>& adapters, LUID luid) {
  for (const Adapter& a : adapters) {
    if (a.luid.LowPart == luid.LowPart && a.luid.HighPart == luid.HighPart) return &a;
  }
  return nullptr;
}

const Adapter* FindByIndexText(const std::vector<Adapter>& adapters, const std::string& wanted) {
  if (wanted.empty() || !std::all_of(wanted.begin(), wanted.end(), ::isdigit)) return nullptr;
  const int index = std::atoi(wanted.c_str());
  for (const Adapter& a : adapters) {
    if (a.index == index) return &a;
  }
  return nullptr;
}

const Adapter* FindByName(const std::vector<Adapter>& adapters, const std::string& wanted) {
  const std::string needle = Lower(wanted);
  for (const Adapter& a : adapters) {
    if (Lower(a.name).find(needle) != std::string::npos) return &a;
  }
  return nullptr;
}

const Adapter* LargestMemory(const std::vector<Adapter>& adapters) {
  const Adapter* best = nullptr;
  for (const Adapter& a : adapters) {
    if (!best || a.dedicated > best->dedicated) best = &a;
  }
  return best;
}

GpuChoice ToChoice(const Adapter* adapter, const char* reason) {
  if (!adapter) return {};
  return {adapter->index, adapter->name, reason};
}

GpuChoice ChooseAuto(IDXGIFactory1* factory, const std::vector<Adapter>& adapters) {
  const std::vector<LUID> order = HighPerformanceOrder(factory);
  for (const LUID& luid : order) {
    const Adapter* a = FindByLuid(adapters, luid);
    if (a && !QueryUma(factory, luid).value_or(false)) return ToChoice(a, "first dedicated GPU in Windows' high-performance order");
  }
  if (!order.empty()) {
    if (const Adapter* a = FindByLuid(adapters, order.front())) return ToChoice(a, "Windows high-performance GPU");
  }
  return ToChoice(LargestMemory(adapters), "largest dedicated memory");
}

GpuChoice ChooseFromSetting(IDXGIFactory1* factory, const std::vector<Adapter>& adapters,
                            const std::string& setting) {
  if (setting.empty() || Lower(setting) == "auto") return ChooseAuto(factory, adapters);
  if (const Adapter* a = FindByIndexText(adapters, setting)) return ToChoice(a, "gow2_gpu index");
  if (const Adapter* a = FindByName(adapters, setting)) return ToChoice(a, "gow2_gpu name");
  GpuChoice fallback = ChooseAuto(factory, adapters);
  fallback.reason = "no adapter matches gow2_gpu '" + setting + "', " + fallback.reason;
  return fallback;
}

}  // namespace

GpuChoice ChooseRenderGpu() {
  IDXGIFactory1* factory = CreateFactory();
  if (!factory) return {};
  const std::vector<Adapter> adapters = ListHardwareAdapters(factory);
  GpuChoice choice = ChooseFromSetting(factory, adapters, REXCVAR_GET(gow2_gpu));
  for (const Adapter& adapter : adapters) {
    if (choice.index >= 0 && adapter.index == choice.index) {
      choice.integrated = QueryIntegrated(factory, adapter);
      break;
    }
  }
  factory->Release();
  return choice;
}

std::vector<GpuAdapterInfo> ListRenderGpus() {
  IDXGIFactory1* factory = CreateFactory();
  if (!factory) return {};
  const std::vector<Adapter> adapters = ListHardwareAdapters(factory);
  factory->Release();
  std::vector<GpuAdapterInfo> out;
  for (const Adapter& adapter : adapters) {
    out.push_back({adapter.index, adapter.name, uint64_t(adapter.dedicated)});
  }
  return out;
}

}  // namespace rex::glue

#else

namespace rex::glue {

GpuChoice ChooseRenderGpu() { return {}; }

std::vector<GpuAdapterInfo> ListRenderGpus() { return {}; }

}  // namespace rex::glue

#endif
