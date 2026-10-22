// Contactica - Windows entry point: window, Direct3D 11, Dear ImGui loop, file dialogs, file drops.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <d3d11.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <filesystem>
#include <string>
#include <vector>
#include "../App.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
using namespace ctc;
std::wstring wide(const std::string& s) { if (s.empty()) return L""; const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0); std::wstring w((size_t)n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n); return w; }
std::string utf8(const std::wstring& w) { if (w.empty()) return ""; const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr); std::string s((size_t)n, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr); return s; }

ID3D11Device* gDevice = nullptr; ID3D11DeviceContext* gContext = nullptr; IDXGISwapChain* gSwap = nullptr; ID3D11RenderTargetView* gRtv = nullptr;
UINT gResizeW = 0, gResizeH = 0; bool gOccluded = false; App* gApp = nullptr; HWND gHwnd = nullptr;
std::vector<std::string>* gDropped = nullptr;

void createRtv() { ID3D11Texture2D* back = nullptr; gSwap->GetBuffer(0, IID_PPV_ARGS(&back)); if (back) { gDevice->CreateRenderTargetView(back, nullptr, &gRtv); back->Release(); } }
void releaseRtv() { if (gRtv) { gRtv->Release(); gRtv = nullptr; } }
bool createDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferDesc.RefreshRate = {60, 1}; sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0}; D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &gSwap, &gDevice, &got, &gContext);
    if (hr == DXGI_ERROR_UNSUPPORTED) hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &gSwap, &gDevice, &got, &gContext);
    if (FAILED(hr)) return false; createRtv(); return true;
}
void destroyDevice() { releaseRtv(); if (gSwap) gSwap->Release(); if (gContext) gContext->Release(); if (gDevice) gDevice->Release(); }

// ---------------------------------------------------------------- the platform
std::wstring filterWide(const char* f) { std::wstring w = wide(f ? f : ""); for (auto& c : w) if (c == L'|') c = L'\0'; w.push_back(L'\0'); w.push_back(L'\0'); return w; }

class WinPlatform : public Platform {
public:
    std::vector<std::string> openFiles(const char* title, const char* filters, bool multi) override {
        std::vector<wchar_t> buf(65536, 0); std::wstring t = wide(title), f = filterWide(filters); OPENFILENAMEW o{}; o.lStructSize = sizeof(o); o.hwndOwner = gHwnd; o.lpstrFilter = f.c_str(); o.lpstrFile = buf.data(); o.nMaxFile = (DWORD)buf.size(); o.lpstrTitle = t.c_str();
        o.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (multi ? OFN_ALLOWMULTISELECT : 0); std::vector<std::string> r; if (!GetOpenFileNameW(&o)) return r;
        const wchar_t* p = buf.data(); const std::wstring first = p; p += first.size() + 1; if (!*p) r.push_back(utf8(first)); else while (*p) { r.push_back(utf8(first + L"\\" + p)); p += wcslen(p) + 1; } return r;
    }
    std::string saveFile(const char* title, const char* filters, const char* defExt, const std::string& suggested) override {
        std::vector<wchar_t> buf(32768, 0); const std::wstring s = wide(suggested); std::copy(s.begin(), s.begin() + std::min(s.size(), buf.size() - 1), buf.begin()); std::wstring t = wide(title), f = filterWide(filters), e = wide(defExt ? defExt : "");
        OPENFILENAMEW o{}; o.lStructSize = sizeof(o); o.hwndOwner = gHwnd; o.lpstrFilter = f.c_str(); o.lpstrFile = buf.data(); o.nMaxFile = (DWORD)buf.size(); o.lpstrTitle = t.c_str(); o.lpstrDefExt = e.c_str(); o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
        if (!GetSaveFileNameW(&o)) return ""; return utf8(buf.data());
    }
    void reveal(const std::string& path) override { std::error_code ec; const bool dir = std::filesystem::is_directory(path, ec); std::wstring a = dir ? L"\"" + wide(path) + L"\"" : L"/select,\"" + wide(path) + L"\""; ShellExecuteW(nullptr, L"open", L"explorer.exe", a.c_str(), nullptr, SW_SHOWNORMAL); }
    void openUrl(const std::string& url) override { ShellExecuteW(nullptr, L"open", wide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL); }
    std::string dataDir() override { PWSTR p = nullptr; std::string r; if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) { r = utf8(p) + "\\Contactica"; CoTaskMemFree(p); } std::error_code ec; std::filesystem::create_directories(r, ec); return r; }
    std::string documentsDir() override { PWSTR p = nullptr; std::string r = "."; if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p))) { r = utf8(p); CoTaskMemFree(p); } return r; }
    void setTitle(const std::string& t) override { if (t != last_) { last_ = t; SetWindowTextW(gHwnd, wide(t).c_str()); } }
private:
    std::string last_;
};

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
    case WM_SIZE: if (wp != SIZE_MINIMIZED) { gResizeW = LOWORD(lp); gResizeH = HIWORD(lp); } return 0;
    case WM_GETMINMAXINFO: { auto* mm = (MINMAXINFO*)lp; mm->ptMinTrackSize = {1000, 640}; return 0; }
    case WM_SYSCOMMAND: if ((wp & 0xfff0) == SC_KEYMENU) return 0; break;
    case WM_DROPFILES: { HDROP d = (HDROP)wp; const UINT n = DragQueryFileW(d, 0xFFFFFFFF, nullptr, 0); for (UINT i = 0; i < n; ++i) { const UINT len = DragQueryFileW(d, i, nullptr, 0); std::wstring f(len, L'\0'); DragQueryFileW(d, i, f.data(), len + 1); if (gDropped) gDropped->push_back(utf8(f)); } DragFinish(d); return 0; }
    case WM_CLOSE: if (!gApp || gApp->wantsClose()) DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    ImGui_ImplWin32_EnableDpiAwareness(); CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, wndProc, 0, 0, inst, LoadIconW(inst, MAKEINTRESOURCEW(1)), LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"ContacticaMain", LoadIconW(inst, MAKEINTRESOURCEW(1))}; RegisterClassExW(&wc);
    gHwnd = CreateWindowExW(0, wc.lpszClassName, L"Contactica", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1600, 960, nullptr, nullptr, inst, nullptr);
    if (!createDevice(gHwnd)) { MessageBoxW(gHwnd, L"Contactica needs Direct3D 11, which this PC could not start.", L"Contactica", MB_ICONERROR); destroyDevice(); return 1; }
    ShowWindow(gHwnd, SW_SHOWMAXIMIZED); UpdateWindow(gHwnd); DragAcceptFiles(gHwnd, TRUE);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; io.IniFilename = nullptr;
    ImGui_ImplWin32_Init(gHwnd); ImGui_ImplDX11_Init(gDevice, gContext);
    const float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(gHwnd); { const char* fonts[] = {"C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\arial.ttf"}; bool ok = false; for (auto* f : fonts) if (!ok && std::filesystem::exists(f)) { io.Fonts->AddFontFromFileTTF(f, 16.f); ok = true; } if (!ok) io.Fonts->AddFontDefault(); }
    {
        std::vector<std::string> dropped; gDropped = &dropped; WinPlatform plat; App app(plat); gApp = &app; ui::applyTheme(dpi);
        int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc); if (argv && argc > 1) { const std::string a = utf8(argv[1]); app.importFiles({a}); } if (argv) LocalFree(argv);
        bool done = false;
        while (!done) {
            MSG msg; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); if (msg.message == WM_QUIT) done = true; }
            if (done) break;
            if (!dropped.empty()) { app.importFiles(dropped); dropped.clear(); }
            if (app.quitRequested) { DestroyWindow(gHwnd); continue; }
            if (gOccluded && gSwap->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) { Sleep(16); continue; } gOccluded = false;
            if (IsIconic(gHwnd)) { Sleep(16); continue; }
            if (gResizeW && gResizeH) { releaseRtv(); gSwap->ResizeBuffers(0, gResizeW, gResizeH, DXGI_FORMAT_UNKNOWN, 0); gResizeW = gResizeH = 0; createRtv(); }
            ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame(); app.frame(); ImGui::Render();
            const float clear[4] = {0.07f, 0.075f, 0.09f, 1.f}; gContext->OMSetRenderTargets(1, &gRtv, nullptr); gContext->ClearRenderTargetView(gRtv, clear); ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            gOccluded = gSwap->Present(1, 0) == DXGI_STATUS_OCCLUDED;
        }
        gApp = nullptr; gDropped = nullptr;
    }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); destroyDevice(); UnregisterClassW(wc.lpszClassName, inst); CoUninitialize(); return 0;
}
