#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "MegaMixPlusFix";
std::string sFixVersion = "0.0.1";
std::filesystem::path sFixPath;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = sFixName + ".log";
std::filesystem::path sExePath;
std::string sExeName;

// Aspect ratio / FOV / HUD
std::pair DesktopDimensions = { 0,0 };
const float fPi = 3.1415926535f;
const float fNativeAspect = 1.7777778f;
float fAspectRatio;
float fAspectMultiplier;
float fHUDWidth;
float fHUDWidthOffset;
float fHUDHeight;
float fHUDHeightOffset;

// Ini variables
bool bFixRes;
bool bStretchHUD;

// Variables
int iCurrentResX;
int iCurrentResY;
std::uint8_t* InternalResList = nullptr;
std::uint8_t* HUDResList = nullptr;

void Logging()
{
    // Get path to DLL
    WCHAR dllPath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
    sFixPath = dllPath;
    sFixPath = sFixPath.remove_filename();

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(exeModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

    // Spdlog initialisation
    try {
        logger = spdlog::basic_logger_st(sFixName.c_str(), sExePath.string() + sLogFile, true);
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::debug);

        spdlog::info("----------");
        spdlog::info("{:s} v{:s} loaded.", sFixName.c_str(), sFixVersion.c_str());
        spdlog::info("----------");
        spdlog::info("Log file: {}", sFixPath.string() + sLogFile);
        spdlog::info("----------");
        spdlog::info("Module Name: {0:s}", sExeName.c_str());
        spdlog::info("Module Path: {0:s}", sExePath.string());
        spdlog::info("Module Address: 0x{0:x}", (uintptr_t)exeModule);
        spdlog::info("Module Timestamp: {0:d}", Memory::ModuleTimestamp(exeModule));
        spdlog::info("----------");
    }
    catch (const spdlog::spdlog_ex& ex) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "Log initialisation failed: " << ex.what() << std::endl;
        FreeLibraryAndExitThread(thisModule, 1);
    }  
}

void Configuration()
{
    // Inipp initialisation
    std::ifstream iniFile(sFixPath / sConfigFile);
    if (!iniFile) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName.c_str() << " v" << sFixVersion.c_str() << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sFixPath.string().c_str() << std::endl;
        spdlog::error("ERROR: Could not locate config file {}", sConfigFile);
        spdlog::shutdown();
        FreeLibraryAndExitThread(thisModule, 1);
    }
    else {
        spdlog::info("Config file: {}", sFixPath.string() + sConfigFile);
        ini.parse(iniFile);
    }

    // Parse config
    ini.strip_trailing_comments();
    spdlog::info("----------");

    // Load settings from ini
    inipp::get_value(ini.sections["Fix Resolution"], "Enabled", bFixRes);
    inipp::get_value(ini.sections["Stretch HUD"], "Enabled", bStretchHUD);

    // Log ini parse
    spdlog_confparse(bFixRes);
    spdlog_confparse(bStretchHUD);

    spdlog::info("----------");
}

void CalculateAspectRatio(bool bLog)
{
    // Check if resolution is invalid
    if (iCurrentResX <= 0 || iCurrentResY <= 0)
        return;

    if (iCurrentResX == 0 || iCurrentResY == 0) {
        spdlog::error("Current Resolution: Resolution invalid, using desktop resolution instead.");
        iCurrentResX = DesktopDimensions.first;
        iCurrentResY = DesktopDimensions.second;
    }

    // Calculate aspect ratio
    fAspectRatio = (float)iCurrentResX / (float)iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD 
    fHUDWidth = (float)iCurrentResY * fNativeAspect;
    fHUDHeight = (float)iCurrentResY;
    fHUDWidthOffset = (float)(iCurrentResX - fHUDWidth) / 2.00f;
    fHUDHeightOffset = 0.00f;
    if (fAspectRatio < fNativeAspect) {
        fHUDWidth = (float)iCurrentResX;
        fHUDHeight = (float)iCurrentResX / fNativeAspect;
        fHUDWidthOffset = 0.00f;
        fHUDHeightOffset = (float)(iCurrentResY - fHUDHeight) / 2.00f;
    }

    // Log details about current resolution
    if (bLog) {
        spdlog::info("----------");
        spdlog::info("Current Resolution: Resolution: {:d}x{:d}", iCurrentResX, iCurrentResY);
        spdlog::info("Current Resolution: fAspectRatio: {}", fAspectRatio);
        spdlog::info("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
        spdlog::info("Current Resolution: fHUDWidth: {}", fHUDWidth);
        spdlog::info("Current Resolution: fHUDHeight: {}", fHUDHeight);
        spdlog::info("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
        spdlog::info("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
        spdlog::info("----------");
    }
}

void Resolution()
{
    // Grab desktop resolution
    DesktopDimensions = Util::GetPhysicalDesktopDimensions();
    iCurrentResX = DesktopDimensions.first;
    iCurrentResY = DesktopDimensions.second;
    CalculateAspectRatio(true);

    if (bFixRes) {
        // Resolution lists
        std::uint8_t* ResolutionListInternalScanResult = Memory::PatternScan(exeModule, "4C 8D ?? ?? ?? ?? ?? 48 ?? ?? ?? 42 ?? ?? ?? 41 ?? ?? ?? 42 ?? ?? ?? ?? 41 ?? ?? ??");
        std::uint8_t* ResolutionListHUDScanResult = Memory::PatternScan(exeModule, "66 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? 41 ?? 03 00 00 00 45 ?? ?? 8B ??");
        if (ResolutionListInternalScanResult && ResolutionListHUDScanResult) {
            spdlog::info("Resolution List: Internal: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListInternalScanResult - (std::uint8_t*)exeModule);
            InternalResList = Memory::GetAbsolute(ResolutionListInternalScanResult + 0x3);

            spdlog::info("Resolution List: HUD: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListHUDScanResult - (std::uint8_t*)exeModule);
            HUDResList = Memory::GetAbsolute(ResolutionListHUDScanResult + 0x4);
        }
        else {
            spdlog::error("Resolution List: Pattern scan(s) failed.");
        }

        // Resolution index
        std::uint8_t* ResolutionIndexStartupScanResult = Memory::PatternScan(exeModule, "41 ?? ?? 7E ?? FF ?? 48 FF ?? 48 83 ?? ?? 7C ?? EB ??");
        std::uint8_t* ResolutionIndexScanResult = Memory::PatternScan(exeModule, "8B ?? ?? ?? 8B ?? ?? ?? C6 44 ?? ?? 00 C6 44 ?? ?? 00");
        if (ResolutionIndexStartupScanResult && ResolutionIndexScanResult) {
            spdlog::info("Resolution Index (Startup): Address is {:s}+{:x}", sExeName.c_str(), ResolutionIndexStartupScanResult - (std::uint8_t*)exeModule);
            static SafetyHookMid ResolutionIndexStartupMidHook{};
            ResolutionIndexStartupMidHook = safetyhook::create_mid(ResolutionIndexStartupScanResult,
                [](SafetyHookContext& ctx) {
                    // Force 3840x2160
                    ctx.rcx = (int)2160;
                });

            spdlog::info("Resolution Index: Address is {:s}+{:x}", sExeName.c_str(), ResolutionIndexScanResult - (std::uint8_t*)exeModule);
            static SafetyHookMid ResolutionIndexMidHook{};
            ResolutionIndexMidHook = safetyhook::create_mid(ResolutionIndexScanResult,
                [](SafetyHookContext& ctx) {
                    // Force 3840x2160
                    ctx.rax = 3;
                });
        }
        else {
            spdlog::error("Resolution Index: Pattern scan(s) failed.");
        }


        // Stop viewport from scaling to 16:9
        std::uint8_t* ViewportSizeScanResult = Memory::PatternScan(exeModule, "89 ?? ?? 48 8B ?? ?? ?? 89 ?? ?? 48 8B ?? ?? ?? 44 ?? ?? ?? 44 ?? ?? ?? 5F C3");
        std::uint8_t* ViewportSizeStartupScanResult = Memory::PatternScan(exeModule, "45 ?? ?? ?? 41 ?? ?? ?? 45 ?? ?? ?? 45 ?? ?? ?? 66 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ??");
        if (ViewportSizeStartupScanResult && ViewportSizeScanResult) {
            spdlog::info("Viewport Size (Startup): Address is {:s}+{:x}", sExeName.c_str(), ViewportSizeStartupScanResult - (std::uint8_t*)exeModule);
            static SafetyHookMid ViewportSizeStartupMidHook{};
            ViewportSizeStartupMidHook = safetyhook::create_mid(ViewportSizeStartupScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.r10 = 0;             // Width offset
                    ctx.r9 = 0;              // Height offset

                    ctx.r8 = ctx.rbx;        // Width
                    ctx.rcx = ctx.rdi;       // Height

                    // Calculate aspect ratio and log resolution
                    int iResX = (int)ctx.r8;
                    int iResY = (int)ctx.rcx;

                    if (iResX != iCurrentResX || iResY != iCurrentResY) {
                        iCurrentResX = iResX;
                        iCurrentResY = iResY;
                        CalculateAspectRatio(true);
                    }

                    // Overwrite 3840x2160 with the current resolution
                    Memory::Write(InternalResList + 0x1A4, iCurrentResX);
                    Memory::Write(InternalResList + 0x1A8, iCurrentResY);

                    Memory::Write(InternalResList + 0x1B8, iCurrentResX);
                    Memory::Write(InternalResList + 0x1BC, iCurrentResY);

                    Memory::Write(HUDResList + 0x8, iCurrentResX);
                    Memory::Write(HUDResList + 0xC, iCurrentResY);
                });

            spdlog::info("Viewport Size: Address is {:s}+{:x}", sExeName.c_str(), ViewportSizeScanResult - (std::uint8_t*)exeModule);
            static SafetyHookMid ViewportSizeMidHook{};
            ViewportSizeMidHook = safetyhook::create_mid(ViewportSizeScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.rbx = 0;            // Width offset
                    ctx.rsi = 0;            // Height offset

                    ctx.r11 = ctx.r10;      // Width
                    ctx.r9 = ctx.r8;        // Height

                    // Calculate aspect ratio and log resolution
                    int iResX = (int)ctx.r11;
                    int iResY = (int)ctx.r9;

                    if (iResX != iCurrentResX || iResY != iCurrentResY) {
                        iCurrentResX = iResX;
                        iCurrentResY = iResY;
                        CalculateAspectRatio(true);
                    }

                    // Overwrite 3840x2160 with the current resolution
                    Memory::Write(InternalResList + 0x1A4, iCurrentResX);
                    Memory::Write(InternalResList + 0x1A8, iCurrentResY);

                    Memory::Write(InternalResList + 0x1B8, iCurrentResX);
                    Memory::Write(InternalResList + 0x1BC, iCurrentResY);

                    Memory::Write(HUDResList + 0x8, iCurrentResX);
                    Memory::Write(HUDResList + 0xC, iCurrentResY);
                });
        }
        else {
            spdlog::error("Viewport Size: Pattern scan(s) failed.");
        }
    }
}

void HUD()
{
    if (!bStretchHUD && bFixRes) {
        // Song selection text
        std::uint8_t* SongSelectTextScanResult = Memory::PatternScan(exeModule, "E8 ?? ?? ?? ?? C7 84 ?? ?? ?? ?? ?? 0D 00 00 00 C7 44 ?? ?? ?? ?? ?? ??");
        if (SongSelectTextScanResult) {
            spdlog::info("HUD: Song Selection Text: Address is {:s}+{:x}", sExeName.c_str(), SongSelectTextScanResult - (std::uint8_t*)exeModule);
            static SafetyHookMid SongSelectTextMidHook{};
            SongSelectTextMidHook = safetyhook::create_mid(SongSelectTextScanResult - 0x5D, // Big offset as it shares a lot of similar code
                [](SafetyHookContext& ctx) {
                    // This is kind of a hack fix that doesn't address the actual problem. 
                    // Fixing the scrolling text would be ideal instead of disabling the horizontal scrolling this way.
                    if (fAspectRatio != fNativeAspect)
                        ctx.rax = 0;
                });
        }
        else {
            spdlog::error("HUD: Song Selection Text: Pattern scan failed.");
        }

        std::uint8_t* SongSelectTextScanResult = Memory::PatternScan(exeModule, "E8 ?? ?? ?? ?? C7 84 ?? ?? ?? ?? ?? 0D 00 00 00 C7 44 ?? ?? ?? ?? ?? ??");
        if (SongSelectTextScanResult) {
            spdlog::info("HUD: Song Selection Text: Address is {:s}+{:x}", sExeName.c_str(), SongSelectTextScanResult - (std::uint8_t*)exeModule);
            static SafetyHookMid SongSelectTextMidHook{};
            SongSelectTextMidHook = safetyhook::create_mid(SongSelectTextScanResult - 0x5D, // Big offset as it shares a lot of similar code
                [](SafetyHookContext& ctx) {
                    
                });
        }
        else {
            spdlog::error("HUD: Song Selection Text: Pattern scan failed.");
        }
    }

    if (bStretchHUD) {
        // HUD width 
        std::uint8_t* HUDWidth1ScanResult = Memory::PatternScan(exeModule, "F3 45 ?? ?? ?? F3 45 ?? ?? ?? F3 41 ?? ?? ?? F3 45 ?? ?? ?? E8 ?? ?? ?? ??");
        std::uint8_t* HUDWidth2ScanResult = Memory::PatternScan(exeModule, "F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? ?? F3 0F ?? ?? ?? 48 8B ?? ?? ?? 48 83 ?? ?? 5F C3");
        if (HUDWidth1ScanResult && HUDWidth2ScanResult) {
            spdlog::info("HUD: Width: 1: Address is {:s}+{:x}", sExeName.c_str(), HUDWidth1ScanResult - (std::uint8_t*)exeModule);
            static SafetyHookMid HUDWidth1MidHook{};
            HUDWidth1MidHook = safetyhook::create_mid(HUDWidth1ScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm11.f32[0] = ctx.xmm9.f32[0] * fNativeAspect;
                    else if (fAspectRatio < fNativeAspect)
                        ctx.xmm9.f32[0] = ctx.xmm11.f32[0] / fNativeAspect;
                });

            spdlog::info("HUD: Width: 2: Address is {:s}+{:x}", sExeName.c_str(), HUDWidth2ScanResult - (std::uint8_t*)exeModule);
            static SafetyHookMid HUDWidth2MidHook{};
            HUDWidth2MidHook = safetyhook::create_mid(HUDWidth2ScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = ctx.xmm1.f32[0] * fNativeAspect;
                    else if (fAspectRatio < fNativeAspect)
                        ctx.xmm1.f32[0] = ctx.xmm0.f32[0] / fNativeAspect;
                });
        }
        else {
            spdlog::error("HUD: Song Selection Text: Pattern scan failed.");
        }
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    Resolution();
    HUD();

    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        thisModule = hModule;

        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle) {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}