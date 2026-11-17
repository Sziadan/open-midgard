#pragma once

#include "DebugLog.h"
#include "SettingsIni.h"

#include <chrono>
#include <cstdlib>
#include <cstring>

class AssetLoadPerf {
public:
    AssetLoadPerf(const char* category, const char* name)
        : m_category(category), m_name(name), m_enabled(Enabled())
    {
        if (m_enabled) {
            m_start = Clock::now();
        }
    }

    void MarkRead()
    {
        if (m_enabled) {
            m_readMs = Milliseconds(Clock::now() - m_start);
        }
    }

    ~AssetLoadPerf()
    {
        if (!m_enabled) {
            return;
        }
        const double totalMs = Milliseconds(Clock::now() - m_start);
        if (totalMs >= 2.0) {
            DbgAlways("[AssetLoadPerf] category=%s name='%s' total=%.3fms read=%.3fms\n",
                m_category, m_name ? m_name : "", totalMs, m_readMs);
        }
    }

private:
    using Clock = std::chrono::steady_clock;

    static bool Enabled()
    {
        static const bool enabled = [] {
            const char* value = std::getenv("RO_ASSET_LOAD_PERF");
            return (value && std::strcmp(value, "1") == 0)
                || LoadSettingsIniInt("Debug", "AssetLoadPerf", 0) != 0;
        }();
        return enabled;
    }

    static double Milliseconds(Clock::duration duration)
    {
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    const char* m_category;
    const char* m_name;
    bool m_enabled;
    Clock::time_point m_start{};
    double m_readMs = 0.0;
};
