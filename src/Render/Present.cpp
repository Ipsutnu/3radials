#include "Present.h"

#include "Logger.h"
#include "RenderManager.h"

#include <REL/Relocation.h>

#include <atomic>
#include <thread>
#include <dxgi.h>
#include <windows.h>

namespace Present
{
    namespace
    {
        using PresentFunction = HRESULT(STDMETHODCALLTYPE*)(
            IDXGISwapChain*, UINT, UINT);

        constexpr std::size_t kPresentVtableIndex = 8;

        IDXGISwapChain* g_swapChain = nullptr;
        std::uintptr_t g_presentSlot = 0;
        PresentFunction g_originalPresent = nullptr;
        bool g_installed = false;
        bool g_loggedFirstPresent = false;
        thread_local bool g_insidePresent = false;

        constexpr ULONGLONG kWatchdogTimeoutMs = 4000;
        std::atomic<bool> g_watchdogRunning{ false };
        std::atomic<const char*> g_watchdogPhase{ nullptr };
        std::atomic<const char*> g_lastReportedPhase{ nullptr };
        std::atomic<ULONGLONG> g_lastPhaseTick{ 0 };
        std::thread g_watchdogThread;

        void WatchdogLoop()
        {
            while (g_watchdogRunning.load(std::memory_order_acquire))
            {
                Sleep(250);

                const char* phase = g_watchdogPhase.load(
                    std::memory_order_acquire);
                if (!phase)
                    continue;

                const auto lastTick = g_lastPhaseTick.load(
                    std::memory_order_acquire);
                const auto now = GetTickCount64();
                if (lastTick == 0 || now - lastTick < kWatchdogTimeoutMs)
                    continue;

                const char* expected = g_lastReportedPhase.load(
                    std::memory_order_relaxed);
                if (expected == phase ||
                    !g_lastReportedPhase.compare_exchange_strong(
                        expected, phase, std::memory_order_acq_rel))
                {
                    continue;
                }

                Logger::GetSingleton().Print(
                    "Present watchdog: no progress for {} ms at '{}'.",
                    now - lastTick, phase);
            }
        }

        void StartWatchdog()
        {
            if (g_watchdogRunning.exchange(true, std::memory_order_acq_rel))
                return;

            g_watchdogPhase.store(nullptr, std::memory_order_release);
            g_lastReportedPhase.store(nullptr, std::memory_order_release);
            g_lastPhaseTick.store(GetTickCount64(), std::memory_order_release);
            g_watchdogThread = std::thread(WatchdogLoop);
            Logger::GetSingleton().Print("Present watchdog: diagnostic active.");
        }

        void StopWatchdog()
        {
            if (!g_watchdogRunning.exchange(false, std::memory_order_acq_rel))
                return;

            if (g_watchdogThread.joinable())
                g_watchdogThread.join();

            g_watchdogPhase.store(nullptr, std::memory_order_release);
            g_lastReportedPhase.store(nullptr, std::memory_order_release);
        }

        HRESULT STDMETHODCALLTYPE PresentThunk(IDXGISwapChain* swapChain,
            UINT syncInterval, UINT flags)
        {
            if (!g_insidePresent && swapChain == g_swapChain)
            {
                if (!g_loggedFirstPresent)
                {
                    Logger::GetSingleton().Print(
                        "Present: final composition hook is receiving frames.");
                    g_loggedFirstPresent = true;
                }
                g_insidePresent = true;
                MarkWatchdogPhase("RenderManager::Present");
                RenderManager::Present();
                g_insidePresent = false;
            }

            if (!g_originalPresent)
                return E_FAIL;

            if (swapChain == g_swapChain)
                MarkWatchdogPhase("original IDXGISwapChain::Present");

            const auto result = g_originalPresent(
                swapChain, syncInterval, flags);

            if (swapChain == g_swapChain)
            {
                MarkWatchdogPhase("RenderManager::AfterPresent");
                RenderManager::AfterPresent();
                MarkWatchdogPhase(nullptr);
            }

            return result;
        }
    }

    bool Install(IDXGISwapChain* swapChain)
    {
        if (g_installed)
            return g_swapChain == swapChain;
        if (!swapChain)
            return false;

        auto** vtable = *reinterpret_cast<void***>(swapChain);
        if (!vtable || !vtable[kPresentVtableIndex])
            return false;

        g_swapChain = swapChain;
        g_presentSlot = reinterpret_cast<std::uintptr_t>(
            &vtable[kPresentVtableIndex]);
        g_originalPresent = reinterpret_cast<PresentFunction>(
            vtable[kPresentVtableIndex]);

        const auto replacement = reinterpret_cast<std::uintptr_t>(
            &PresentThunk);
        REL::safe_write(g_presentSlot, replacement);

        g_installed = true;
        g_loggedFirstPresent = false;
        StartWatchdog();
        Logger::GetSingleton().Print(
            "Present: final composition hook installed on the active swap chain.");
        return true;
    }

    void Uninstall()
    {
        if (!g_installed)
            return;

        const auto current = *reinterpret_cast<std::uintptr_t*>(g_presentSlot);
        if (current == reinterpret_cast<std::uintptr_t>(&PresentThunk) &&
            g_originalPresent)
        {
            const auto original = reinterpret_cast<std::uintptr_t>(
                g_originalPresent);
            REL::safe_write(g_presentSlot, original);
        }

        g_swapChain = nullptr;
        g_presentSlot = 0;
        g_originalPresent = nullptr;
        g_installed = false;
        g_loggedFirstPresent = false;
        StopWatchdog();
    }

    bool IsInstalled()
    {
        return g_installed;
    }

    void MarkWatchdogPhase(const char* phase) noexcept
    {
        g_watchdogPhase.store(phase, std::memory_order_release);
        g_lastPhaseTick.store(GetTickCount64(), std::memory_order_release);
        if (!phase)
            g_lastReportedPhase.store(nullptr, std::memory_order_release);
    }
}
