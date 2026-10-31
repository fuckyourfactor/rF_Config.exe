#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <d3d9.h>

#include "config.hpp"
#include "resource.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
namespace {
constexpr char kSection[] = "COMPONENTS";

struct Resolution {
    UINT width = 0;
    UINT height = 0;
    int depth = 32;
    int gameIndex = 0;
    std::set<int> gameIndices;
    std::set<UINT> refreshRates;
};

struct Adapter {
    UINT ordinal = 0;
    D3DADAPTER_IDENTIFIER9 identifier{};
    std::wstring description;
    std::wstring guid;
    std::vector<Resolution> resolutions;
};

struct Language {
    std::wstring displayName;
    std::string file;
    std::string commentary;
};

std::wstring widen(const std::string& text) {
    if (text.empty()) return L"";
    int size = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
    if (size < 1) return L"";
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, result.data(), size);
    result.resize(size - 1);
    return result;
}

std::string narrow(const std::wstring& text) {
    if (text.empty()) return "";
    int size = WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size < 1) return "";
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr);
    result.resize(size - 1);
    return result;
}

std::wstring guidString(const GUID& id) {
    wchar_t output[64];
    swprintf_s(output, L"%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
               id.Data1, id.Data2, id.Data3,
               id.Data4[0], id.Data4[1], id.Data4[2], id.Data4[3],
               id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
    return output;
}

std::wstring lower(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towlower(c));
    });
    return text;
}

std::wstring exeDirectory() {
    std::vector<wchar_t> path(32768);
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return L".";
    return fs::path(std::wstring(path.data(), length)).parent_path().wstring();
}

std::wstring selectedPath(int argc, wchar_t** argv, const fs::path& root) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        std::wstring folded = lower(arg);
        if ((folded == L"--config" || folded == L"-config") && i + 1 < argc) {
            fs::path path = argv[i + 1];
            return (path.is_absolute() ? path : root / path).wstring();
        }
        if (folded.rfind(L"config=", 0) == 0) {
            fs::path path = arg.substr(7);
            return (path.is_absolute() ? path : root / path).wstring();
        }
    }
    return (root / L"Config.ini").wstring();
}

std::wstring comboString(HWND window, int id, int index) {
    auto length = static_cast<int>(SendDlgItemMessageW(window, id, CB_GETLBTEXTLEN, index, 0));
    if (length < 0) return L"";
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    SendDlgItemMessageW(window, id, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<size_t>(length));
    return text;
}

int comboSelection(HWND window, int id) {
    return static_cast<int>(SendDlgItemMessageW(window, id, CB_GETCURSEL, 0, 0));
}

void chooseCombo(HWND window, int id, int index) {
    SendDlgItemMessageW(window, id, CB_SETCURSEL, index, 0);
}

void addCombo(HWND window, int id, const std::wstring& text) {
    SendDlgItemMessageW(window, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
}

int screenDepth(D3DFORMAT format) {
    return format == D3DFMT_R5G6B5 || format == D3DFMT_X1R5G5B5 ? 16 : 32;
}

std::pair<UINT, UINT> sizeSetting(const ConfigFile& config, const char* key, UINT defWidth, UINT defHeight) {
    std::string value = config.get(kSection, key);
    unsigned width = 0, height = 0;
    if (std::sscanf(value.c_str(), " ( %u , %u ) ", &width, &height) == 2 && width > 0 && height > 0)
        return {width, height};
    return {defWidth, defHeight};
}

class ConfigWindow {
public:
    ConfigWindow(HINSTANCE instance, std::wstring configPath, fs::path root, bool debug)
        : instance_(instance), configPath_(std::move(configPath)), root_(std::move(root)), debug_(debug) {}

    int run() {
        configExisted_ = config_.load(configPath_);
        if (!configExisted_) config_.addDefaults();
        direct3d_ = Direct3DCreate9(D3D_SDK_VERSION);
        if (direct3d_) enumerateAdapters();
        INT_PTR result = DialogBoxParamW(instance_, MAKEINTRESOURCEW(IDD_MAIN), nullptr, dialogProc,
                                         reinterpret_cast<LPARAM>(this));
        if (direct3d_) direct3d_->Release();
        return result == -1 ? 1 : 0;
    }

private:
    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    ConfigFile config_;
    std::wstring configPath_;
    fs::path root_;
    bool debug_ = false;
    bool configExisted_ = false;
    IDirect3D9* direct3d_ = nullptr;
    std::vector<Adapter> adapters_;
    std::vector<Language> languages_;
    std::vector<std::pair<std::wstring, int>> aaOptions_;
    std::vector<UINT> refreshOptions_;
    bool driverChanged_ = false;
    bool modeChanged_ = false;
    bool refreshChanged_ = false;
    bool fsaaChanged_ = false;
    bool shaderChanged_ = false;
    bool languageChanged_ = false;
    bool syncChanged_ = false;
    bool windowedChanged_ = false;
    bool multiviewChanged_ = false;
    bool widescreenChanged_ = false;

    static INT_PTR CALLBACK dialogProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        ConfigWindow* app = reinterpret_cast<ConfigWindow*>(GetWindowLongPtrW(window, DWLP_USER));
        if (message == WM_INITDIALOG) {
            app = reinterpret_cast<ConfigWindow*>(lparam);
            SetWindowLongPtrW(window, DWLP_USER, reinterpret_cast<LONG_PTR>(app));
            app->window_ = window;
            app->initialize();
            return TRUE;
        }
        if (!app) return FALSE;
        if (message == WM_COMMAND) return app->command(LOWORD(wparam), HIWORD(wparam));
        if (message == WM_CLOSE) { EndDialog(window, IDCANCEL); return TRUE; }
        if (message == WM_CTLCOLORSTATIC) {
            auto id = GetDlgCtrlID(reinterpret_cast<HWND>(lparam));
            if (id == IDC_LINK_SUPPORT || id == IDC_LINK_DIRECTX || id == IDC_LINK_AMD || id == IDC_LINK_NVIDIA) {
                SetTextColor(reinterpret_cast<HDC>(wparam), RGB(0, 85, 180));
                SetBkMode(reinterpret_cast<HDC>(wparam), TRANSPARENT);
                return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
            }
        }
        return FALSE;
    }

    Adapter* selectedAdapter() {
        int index = comboSelection(window_, IDC_DRIVER);
        return index >= 0 && static_cast<size_t>(index) < adapters_.size() ? &adapters_[index] : nullptr;
    }

    Resolution* selectedResolution() {
        auto* adapter = selectedAdapter();
        int index = comboSelection(window_, IDC_RESOLUTION);
        return adapter && index >= 0 && static_cast<size_t>(index) < adapter->resolutions.size()
            ? &adapter->resolutions[index] : nullptr;
    }

    void enumerateAdapters() {
        UINT count = direct3d_->GetAdapterCount();
        auto minimum = sizeSetting(config_, "MinimumVidRes", 800, 600);
        auto maximum = sizeSetting(config_, "MaximumVidRes", 8000, 6000);
        for (UINT a = 0; a < count; ++a) {
            Adapter adapter;
            adapter.ordinal = a;
            if (FAILED(direct3d_->GetAdapterIdentifier(a, 0, &adapter.identifier))) continue;
            adapter.description = widen(adapter.identifier.Description);
            adapter.guid = guidString(adapter.identifier.DeviceIdentifier);
            std::map<std::tuple<UINT, UINT, int>, Resolution> modes;
            int gameIndex = 0;
            for (D3DFORMAT format : {D3DFMT_X8R8G8B8, D3DFMT_R5G6B5}) {
                UINT modeCount = direct3d_->GetAdapterModeCount(a, format);
                for (UINT i = 0; i < modeCount; ++i) {
                    D3DDISPLAYMODE mode{};
                    if (FAILED(direct3d_->EnumAdapterModes(a, format, i, &mode))) continue;
                    if (mode.Width < minimum.first || mode.Height < minimum.second ||
                        mode.Width > maximum.first || mode.Height > maximum.second) continue;
                    int depth = screenDepth(mode.Format);
                    auto key = std::make_tuple(mode.Width, mode.Height, depth);
                    int enumeratedMode = gameIndex++;
                    auto entry = modes.find(key);
                    if (entry == modes.end()) {
                        Resolution resolution;
                        resolution.width = mode.Width;
                        resolution.height = mode.Height;
                        resolution.depth = depth;
                        resolution.gameIndex = enumeratedMode;
                        entry = modes.emplace(key, resolution).first;
                    }
                    entry->second.gameIndices.insert(enumeratedMode);
                    if (mode.RefreshRate) entry->second.refreshRates.insert(mode.RefreshRate);
                }
            }
            for (auto& entry : modes) adapter.resolutions.push_back(std::move(entry.second));
            std::sort(adapter.resolutions.begin(), adapter.resolutions.end(), [](const auto& lhs, const auto& rhs) {
                return std::tie(lhs.width, lhs.height, lhs.depth) < std::tie(rhs.width, rhs.height, rhs.depth);
            });
            adapters_.push_back(std::move(adapter));
        }
    }

    void enumerateLanguages() {
        languages_.push_back({L"English-USA", "GAME.DIC", "CommentaryEnglish-USA.ini"});
        fs::path languagesDir = widen(config_.get(kSection, "LanguagesDir", "Support\\Languages\\"));
        fs::path fullPath = languagesDir.is_absolute() ? languagesDir : root_ / languagesDir;
        std::error_code ec;
        if (fs::is_directory(fullPath, ec)) {
            for (const auto& entry : fs::directory_iterator(fullPath, ec)) {
                if (ec) break;
                if (!entry.is_regular_file(ec) || lower(entry.path().extension().wstring()) != L".dic") continue;
                std::string file = narrow(entry.path().filename().wstring());
                std::wstring stem = entry.path().stem().wstring();
                if (lower(stem) == L"game") continue;
                languages_.push_back({stem, file, "Commentary" + narrow(stem) + ".ini"});
            }
        }
        std::string configured = config_.get(kSection, "LanguagesFile", "GAME.DIC");
        bool found = false;
        for (const auto& language : languages_)
            if (lower(widen(language.file)) == lower(widen(configured))) found = true;
        if (!found) {
            std::wstring filename = widen(configured);
            languages_.push_back({fs::path(filename).stem().wstring(), configured,
                                  config_.get(kSection, "CommentaryFile")});
        }
        for (const auto& language : languages_) addCombo(window_, IDC_LANGUAGE, language.displayName);
        for (size_t i = 0; i < languages_.size(); ++i) {
            if (lower(widen(languages_[i].file)) == lower(widen(configured))) {
                chooseCombo(window_, IDC_LANGUAGE, static_cast<int>(i));
                break;
            }
        }
    }

    void initialize() {
        SendMessageW(window_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadIconW(nullptr, IDI_APPLICATION)));
        addCombo(window_, IDC_SHADER, L"Auto");
        addCombo(window_, IDC_SHADER, L"Quality (DX9)");
        addCombo(window_, IDC_SHADER, L"Balanced (DX8)");
        addCombo(window_, IDC_SHADER, L"Performance (DX7)");
        const int levels[] = {0, 3, 2, 1};
        int storedShader = config_.getInt(kSection, "ShaderLevel", 3);
        int selectedShader = 0;
        for (int i = 0; i < 4; ++i)
            if (levels[i] == storedShader) selectedShader = i;
        chooseCombo(window_, IDC_SHADER, selectedShader);
        CheckDlgButton(window_, IDC_VSYNC, config_.getInt(kSection, "VSync", 0) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(window_, IDC_WINDOWED, config_.getInt(kSection, "WindowedMode", 0) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(window_, IDC_MULTIVIEW, config_.getInt(kSection, "SubViews", 0) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(window_, IDC_WIDESCREEN, config_.getInt(kSection, "WidescreenUI", 1) ? BST_CHECKED : BST_UNCHECKED);
        enumerateLanguages();
        if (!configExisted_) populateHardwareDefaults();
        for (const auto& adapter : adapters_) addCombo(window_, IDC_DRIVER, adapter.description);
        if (adapters_.empty()) {
            addCombo(window_, IDC_DRIVER, L"No Direct3D 9 adapter found");
            chooseCombo(window_, IDC_DRIVER, 0);
            EnableWindow(GetDlgItem(window_, IDC_DRIVER), FALSE);
            EnableWindow(GetDlgItem(window_, IDC_RESOLUTION), FALSE);
            EnableWindow(GetDlgItem(window_, IDC_REFRESH), FALSE);
            EnableWindow(GetDlgItem(window_, IDC_FSAA), FALSE);
            addCombo(window_, IDC_RESOLUTION, L"Unavailable");
            chooseCombo(window_, IDC_RESOLUTION, 0);
            addCombo(window_, IDC_REFRESH, L"Default");
            chooseCombo(window_, IDC_REFRESH, 0);
            addCombo(window_, IDC_FSAA, L"None");
            chooseCombo(window_, IDC_FSAA, 0);
        } else {
            int selection = config_.getInt(kSection, "VideoDriver", 0);
            std::wstring savedGUID = lower(widen(config_.get(kSection, "VideoGUID")));
            for (size_t i = 0; i < adapters_.size(); ++i) {
                if (!savedGUID.empty() && lower(adapters_[i].guid) == savedGUID) {
                    selection = static_cast<int>(i);
                    break;
                }
            }
            if (selection < 0 || selection >= static_cast<int>(adapters_.size())) selection = 0;
            chooseCombo(window_, IDC_DRIVER, selection);
            fillResolutions(true);
        }
        setSystemInfo();
        if (debug_) showDebug();
    }

    void fillResolutions(bool useSaved) {
        SendDlgItemMessageW(window_, IDC_RESOLUTION, CB_RESETCONTENT, 0, 0);
        auto* adapter = selectedAdapter();
        if (!adapter) return;
        int selection = -1;
        int savedMode = config_.getInt(kSection, "VideoMode", -1);
        D3DDISPLAYMODE desktop{};
        direct3d_->GetAdapterDisplayMode(adapter->ordinal, &desktop);
        int desktopSelection = -1;
        for (size_t i = 0; i < adapter->resolutions.size(); ++i) {
            auto& r = adapter->resolutions[i];
            wchar_t label[128];
            swprintf_s(label, L"%u x %u %d bit", r.width, r.height, r.depth);
            addCombo(window_, IDC_RESOLUTION, label);
            if (useSaved && r.gameIndices.count(savedMode)) selection = static_cast<int>(i);
            if (r.width == desktop.Width && r.height == desktop.Height && r.depth == screenDepth(desktop.Format))
                desktopSelection = static_cast<int>(i);
        }
        if (selection < 0) selection = desktopSelection >= 0 ? desktopSelection : 0;
        if (!adapter->resolutions.empty()) chooseCombo(window_, IDC_RESOLUTION, selection);
        fillRefresh(useSaved);
        fillAA(useSaved);
    }

    void fillRefresh(bool useSaved) {
        SendDlgItemMessageW(window_, IDC_REFRESH, CB_RESETCONTENT, 0, 0);
        refreshOptions_.clear();
        refreshOptions_.push_back(0);
        addCombo(window_, IDC_REFRESH, L"Default");
        auto* resolution = selectedResolution();
        if (resolution) {
            for (auto rate : resolution->refreshRates) {
                if (rate == 0) continue;
                refreshOptions_.push_back(rate);
                addCombo(window_, IDC_REFRESH, std::to_wstring(rate) + L" Hz");
            }
        }
        int current = useSaved ? config_.getInt(kSection, "VideoRefresh", 0) : 0;
        if (current < 0 || current >= static_cast<int>(refreshOptions_.size())) current = 0;
        chooseCombo(window_, IDC_REFRESH, current);
    }

    void fillAA(bool useSaved) {
        SendDlgItemMessageW(window_, IDC_FSAA, CB_RESETCONTENT, 0, 0);
        aaOptions_.clear();
        aaOptions_.emplace_back(L"None", 0);
        auto* adapter = selectedAdapter();
        auto* resolution = selectedResolution();
        if (adapter && resolution && direct3d_) {
            auto format = resolution->depth == 16 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
            BOOL windowed = IsDlgButtonChecked(window_, IDC_WINDOWED) == BST_CHECKED;
            for (int n = 2; n <= 16; ++n) {
                DWORD qualityLevels = 0;
                HRESULT hr = direct3d_->CheckDeviceMultiSampleType(adapter->ordinal, D3DDEVTYPE_HAL,
                                 format, windowed, static_cast<D3DMULTISAMPLE_TYPE>(n), &qualityLevels);
                if (SUCCEEDED(hr)) {
                    aaOptions_.emplace_back(std::to_wstring(n) + L"x MultiSample", n);
                }
            }
            for (int level = 1; level <= 8; ++level) {
                aaOptions_.emplace_back(L"Level " + std::to_wstring(level), 31 + level);
            }
        }
        int saved = config_.getInt(kSection, "FSAA", 0);
        int selection = 0;
        bool found = false;
        for (size_t i = 0; i < aaOptions_.size(); ++i) {
            if (aaOptions_[i].second == saved) { selection = static_cast<int>(i); found = true; break; }
        }
        if (useSaved && !found && saved != 0) {
            aaOptions_.emplace_back(L"Existing value (" + std::to_wstring(saved) + L")", saved);
            selection = static_cast<int>(aaOptions_.size()) - 1;
        }
        for (const auto& item : aaOptions_) addCombo(window_, IDC_FSAA, item.first);
        chooseCombo(window_, IDC_FSAA, useSaved ? selection : 0);
    }

    void populateHardwareDefaults() {
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        if (GlobalMemoryStatusEx(&memory)) {
            auto mb = memory.ullTotalPhys / (1024ULL * 1024ULL);
            config_.setInt(kSection, "SystemRAM", static_cast<int>(std::min<ULONGLONG>(mb, 4095)));
        }
        if (!direct3d_ || adapters_.empty()) return;
        D3DPRESENT_PARAMETERS present{};
        present.Windowed = TRUE;
        present.SwapEffect = D3DSWAPEFFECT_DISCARD;
        present.hDeviceWindow = window_;
        IDirect3DDevice9* device = nullptr;
        auto result = direct3d_->CreateDevice(0, D3DDEVTYPE_HAL, window_,
                          D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &device);
        if (SUCCEEDED(result) && device) {
            UINT mb = device->GetAvailableTextureMem() / (1024U * 1024U);
            if (mb) config_.setInt(kSection, "VideoRAM", static_cast<int>(std::min(mb, 4095U)));
            device->Release();
        }
    }

    void setSystemInfo() {
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        GlobalMemoryStatusEx(&memory);
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        std::wstring cpu = L"CPU: " + std::to_wstring(system.dwNumberOfProcessors) + L" logical processors";
        wchar_t brand[256]{};
        DWORD bytes = sizeof(brand);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                         L"ProcessorNameString", RRF_RT_REG_SZ, nullptr, brand, &bytes) == ERROR_SUCCESS) {
            cpu = std::wstring(L"CPU: ") + brand;
        }
        cpu += L" | RAM: " + std::to_wstring(memory.ullTotalPhys / (1024ULL * 1024ULL)) + L" MB";
        SetDlgItemTextW(window_, IDC_INFO_CPU, cpu.c_str());
        OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
#pragma warning(push)
#pragma warning(disable:4996)
        GetVersionExW(&version);
#pragma warning(pop)
        auto os = L"Windows " + std::to_wstring(version.dwMajorVersion) + L"." +
                  std::to_wstring(version.dwMinorVersion) + L" (build " + std::to_wstring(version.dwBuildNumber) + L")";
        SetDlgItemTextW(window_, IDC_INFO_OS, os.c_str());
        auto* adapter = selectedAdapter();
        if (!adapter) {
            SetDlgItemTextW(window_, IDC_INFO_GPU, L"GPU: No Direct3D 9 adapter available");
            SetDlgItemTextW(window_, IDC_INFO_DRIVER, L"Video Driver: unavailable");
        } else {
            SetDlgItemTextW(window_, IDC_INFO_GPU, (L"GPU: " + adapter->description).c_str());
            auto versionInfo = adapter->identifier.DriverVersion;
            std::wstring driver = L"Video Driver: " + widen(adapter->identifier.Driver) + L" v" +
                std::to_wstring(HIWORD(versionInfo.HighPart)) + L"." +
                std::to_wstring(LOWORD(versionInfo.HighPart)) + L"." +
                std::to_wstring(HIWORD(versionInfo.LowPart)) + L"." +
                std::to_wstring(LOWORD(versionInfo.LowPart));
            SetDlgItemTextW(window_, IDC_INFO_DRIVER, driver.c_str());
        }
        SetDlgItemTextW(window_, IDC_INFO_DX,
                        direct3d_ ? L"Direct3D: version 9 available" : L"Direct3D 9 is unavailable");
        SetDlgItemTextW(window_, IDC_INFO_FILE, (L"File: " + configPath_).c_str());
    }

    void videoTest() {
        std::wstring message;
        if (!direct3d_) {
            message = L"Direct3D 9 is not installed or could not be initialized.";
        } else if (!selectedAdapter()) {
            message = L"No Direct3D 9 video adapter is available.";
        } else {
            auto* adapter = selectedAdapter();
            D3DCAPS9 caps{};
            HRESULT hr = direct3d_->GetDeviceCaps(adapter->ordinal, D3DDEVTYPE_HAL, &caps);
            if (FAILED(hr)) {
                message = L"The selected adapter does not expose Direct3D 9 hardware capabilities.";
            } else {
                HRESULT dxt = direct3d_->CheckDeviceFormat(adapter->ordinal, D3DDEVTYPE_HAL,
                                 D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT1);
                if (FAILED(dxt)) message = L"The adapter does not report support for DXT1 compressed textures.";
                else {
                    D3DPRESENT_PARAMETERS present{};
                    present.Windowed = TRUE;
                    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
                    present.hDeviceWindow = window_;
                    IDirect3DDevice9* device = nullptr;
                    hr = direct3d_->CreateDevice(adapter->ordinal, D3DDEVTYPE_HAL, window_,
                               D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &device);
                    if (SUCCEEDED(hr) && device) {
                        device->Release();
                        message = L"Direct3D 9 initialized successfully. DXT texture support is present.\n\n"
                                  L"This test does not guarantee that every rFactor mod will run.";
                    } else {
                        wchar_t error[80];
                        swprintf_s(error, L"Could not create a Direct3D device (HRESULT 0x%08lX).", hr);
                        message = error;
                    }
                }
            }
        }
        MessageBoxW(window_, message.c_str(), L"rFactor Video Test", MB_OK | MB_ICONINFORMATION);
    }

    void sendStats() {
        const fs::path options[] = {root_ / L"statsend.exe", root_ / L"Support" / L"statsend.exe"};
        for (const auto& file : options) {
            std::error_code ec;
            if (!fs::is_regular_file(file, ec)) continue;
            auto executable = file.wstring();
            HINSTANCE result = ShellExecuteW(window_, L"open", executable.c_str(), L"-sendstats",
                                            root_.c_str(), SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) > 32) return;
            MessageBoxW(window_, L"Could not start statsend.exe.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }
        MessageBoxW(window_, L"statsend.exe was not found in the rFactor or Support directory.",
                    L"Error", MB_OK | MB_ICONERROR);
    }

    static INT_PTR CALLBACK debugDialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        if (msg == WM_INITDIALOG) {
            static const wchar_t* errors[] = {
                L"Wrong DirectX", L"gMotor Wont Start", L"No DirectX Adapter",
                L"Unsupported OS", L"Not Enough CPU", L"Not Enough Sys RAM",
                L"Not Enough Vid Ram", L"Not Enough Swap", L"No DXT Compress",
                L"No Sound Card", L"All Good"
            };
            for (const wchar_t* error : errors)
                SendDlgItemMessageW(hwnd, IDC_DEBUG_ERRORS, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(error));
            SendDlgItemMessageW(hwnd, IDC_DEBUG_ERRORS, LB_SETCURSEL, 10, 0);
            return TRUE;
        }
        if (msg == WM_COMMAND) {
            if (LOWORD(wparam) == IDOK) {
                int selected = static_cast<int>(SendDlgItemMessageW(hwnd, IDC_DEBUG_ERRORS, LB_GETCURSEL, 0, 0));
                if (selected >= 0) {
                    wchar_t choice[128]{};
                    SendDlgItemMessageW(hwnd, IDC_DEBUG_ERRORS, LB_GETTEXT, selected, reinterpret_cast<LPARAM>(choice));
                    MessageBoxW(hwnd, choice, L"Simulated video test outcome", MB_OK | MB_ICONINFORMATION);
                }
                EndDialog(hwnd, IDOK);
                return TRUE;
            }
            if (LOWORD(wparam) == IDCANCEL) { EndDialog(hwnd, IDCANCEL); return TRUE; }
        }
        return FALSE;
    }

    void showDebug() {
        DialogBoxParamW(instance_, MAKEINTRESOURCEW(IDD_DEBUG), window_, debugDialogProc, 0);
    }

    bool saveSettings() {
        if (!configExisted_ || driverChanged_) {
            if (auto* adapter = selectedAdapter()) {
                config_.setInt(kSection, "VideoDriver", static_cast<int>(adapter->ordinal));
                config_.set(kSection, "VideoGUID", narrow(adapter->guid));
            }
        }
        if (!configExisted_ || modeChanged_) {
            if (auto* resolution = selectedResolution())
                config_.setInt(kSection, "VideoMode", resolution->gameIndex);
        }
        if (!configExisted_ || refreshChanged_) {
            int selection = comboSelection(window_, IDC_REFRESH);
            if (selection >= 0) config_.setInt(kSection, "VideoRefresh", selection);
        }
        if (!configExisted_ || fsaaChanged_) {
            int selection = comboSelection(window_, IDC_FSAA);
            if (selection >= 0 && selection < static_cast<int>(aaOptions_.size()))
                config_.setInt(kSection, "FSAA", aaOptions_[selection].second);
        }
        if (!configExisted_ || shaderChanged_) {
            int selection = comboSelection(window_, IDC_SHADER);
            const int values[] = {0, 3, 2, 1};
            if (selection >= 0 && selection < 4) config_.setInt(kSection, "ShaderLevel", values[selection]);
        }
        if (!configExisted_ || languageChanged_) {
            int selection = comboSelection(window_, IDC_LANGUAGE);
            if (selection >= 0 && selection < static_cast<int>(languages_.size())) {
                config_.set(kSection, "LanguagesFile", languages_[selection].file);
                config_.set(kSection, "CommentaryFile", languages_[selection].commentary);
            }
        }
        if (!configExisted_ || syncChanged_)
            config_.setInt(kSection, "VSync", IsDlgButtonChecked(window_, IDC_VSYNC) == BST_CHECKED ? 1 : 0);
        if (!configExisted_ || windowedChanged_)
            config_.setInt(kSection, "WindowedMode", IsDlgButtonChecked(window_, IDC_WINDOWED) == BST_CHECKED ? 1 : 0);
        if (!configExisted_ || multiviewChanged_)
            config_.setInt(kSection, "SubViews", IsDlgButtonChecked(window_, IDC_MULTIVIEW) == BST_CHECKED ? 1 : 0);
        if (!configExisted_ || widescreenChanged_)
            config_.setInt(kSection, "WidescreenUI", IsDlgButtonChecked(window_, IDC_WIDESCREEN) == BST_CHECKED ? 1 : 0);
        std::string error;
        if (!config_.save(configPath_, error)) {
            std::wstring details = L"Unable to save rFactor configuration.\n" + widen(error);
            MessageBoxW(window_, details.c_str(), L"Save failed", MB_OK | MB_ICONERROR);
            return false;
        }
        return true;
    }

    INT_PTR command(int id, int notification) {
        if (notification == STN_CLICKED) {
            const wchar_t* destination = nullptr;
            if (id == IDC_LINK_SUPPORT) destination = L"https://www.overtake.gg/forums/rfactor.10/";
            if (id == IDC_LINK_DIRECTX) destination = L"https://www.microsoft.com/en-us/download/details.aspx?id=35";
            if (id == IDC_LINK_AMD) destination = L"https://www.amd.com/en/support";
            if (id == IDC_LINK_NVIDIA) destination = L"https://www.nvidia.com/en-us/drivers/";
            if (destination) { ShellExecuteW(window_, L"open", destination, nullptr, nullptr, SW_SHOWNORMAL); return TRUE; }
        }
        if (notification == CBN_SELCHANGE) {
            switch (id) {
                case IDC_DRIVER:
                    driverChanged_ = modeChanged_ = refreshChanged_ = fsaaChanged_ = true;
                    fillResolutions(false);
                    setSystemInfo();
                    return TRUE;
                case IDC_RESOLUTION:
                    modeChanged_ = refreshChanged_ = fsaaChanged_ = true;
                    fillRefresh(false);
                    fillAA(false);
                    return TRUE;
                case IDC_SHADER: shaderChanged_ = true; return TRUE;
                case IDC_REFRESH: refreshChanged_ = true; return TRUE;
                case IDC_FSAA: fsaaChanged_ = true; return TRUE;
                case IDC_LANGUAGE: languageChanged_ = true; return TRUE;
                default: break;
            }
        }
        if (notification == BN_CLICKED) {
            switch (id) {
                case IDC_VSYNC: syncChanged_ = true; return TRUE;
                case IDC_WINDOWED:
                    windowedChanged_ = fsaaChanged_ = true;
                    fillAA(true);
                    return TRUE;
                case IDC_WIDESCREEN: widescreenChanged_ = true; return TRUE;
                case IDC_MULTIVIEW: multiviewChanged_ = true; return TRUE;
                case IDC_TEST: videoTest(); return TRUE;
                case IDC_SEND: sendStats(); return TRUE;
                case IDOK: if (saveSettings()) EndDialog(window_, IDOK); return TRUE;
                case IDCANCEL: EndDialog(window_, IDCANCEL); return TRUE;
                default: break;
            }
        }
        return FALSE;
    }
};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    fs::path root = exeDirectory();
    std::wstring configPath = selectedPath(argc, argv, root);
    bool debug = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring option = lower(argv[i]);
        if (option == L"-debug" || option == L"--debug") debug = true;
    }
    if (argv) LocalFree(argv);
    ConfigWindow app(instance, configPath, root, debug);
    return app.run();
}
