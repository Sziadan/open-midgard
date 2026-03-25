#include "Audio.h"
#include "core/DllMgr.h"

#include <algorithm>

CAudio::CAudio()
    : m_digDriver(nullptr),
      m_volume(1.0f),
      m_bgmStream(nullptr),
      m_bgmVolume(127),
      m_bgmPaused(false)
{
}

CAudio* CAudio::GetInstance() {
    static CAudio instance;
    return &instance;
}

bool CAudio::Init() {
    if (m_digDriver) {
        return true;
    }

    if (!g_dllExports.AIL_startup) return false;

    g_dllExports.AIL_startup();
    m_digDriver = g_dllExports.AIL_open_digital_driver(44100, 16, 2, 0);
    return m_digDriver != nullptr;
}

void CAudio::PlaySound(const char* fName, float vol) {
    (void)fName;

    if (!m_digDriver || !g_dllExports.AIL_allocate_sample_handle || !g_soundMode) return;

    const float clampedVolume = (std::max)(0.0f, (std::min)(1.0f, vol * m_volume));
    if (clampedVolume <= 0.0f) {
        return;
    }

    void* sample = g_dllExports.AIL_allocate_sample_handle(m_digDriver);
    if (sample) {
        g_dllExports.AIL_init_sample(sample);
        // In a real implementation, we would load the file data into a buffer first
        // For now, this is a placeholder for the wiring logic
        // g_dllExports.AIL_set_sample_file(sample, data, -1);
        if (g_dllExports.AIL_set_sample_volume) {
            g_dllExports.AIL_set_sample_volume(sample, static_cast<int>(clampedVolume * 127.0f));
        }
        g_dllExports.AIL_start_sample(sample);
    }
}

void CAudio::PlayBGM(const char* fName) {
    if (!m_digDriver || !g_dllExports.AIL_open_stream) return;

    StopBGM();

    void* stream = g_dllExports.AIL_open_stream(m_digDriver, fName, 0);
    if (stream) {
        m_bgmStream = stream;
        if (g_dllExports.AIL_set_stream_loop_count) {
            g_dllExports.AIL_set_stream_loop_count(stream, 0);
        }
        if (g_dllExports.AIL_set_stream_volume) {
            g_dllExports.AIL_set_stream_volume(stream, m_bgmVolume);
        }
        if (g_dllExports.AIL_pause_stream) {
            g_dllExports.AIL_pause_stream(stream, m_bgmPaused ? 1 : 0);
        }
        g_dllExports.AIL_start_stream(stream);
    }
}

void CAudio::StopBGM() {
    if (m_bgmStream && g_dllExports.AIL_close_stream) {
        g_dllExports.AIL_close_stream(m_bgmStream);
    }
    m_bgmStream = nullptr;
}

void CAudio::SetVolume(float vol) {
    m_volume = (std::max)(0.0f, (std::min)(1.0f, vol));
}

float CAudio::GetVolume() const {
    return m_volume;
}

void CAudio::SetBgmVolume(int vol)
{
    m_bgmVolume = (std::max)(0, (std::min)(127, vol));
    if (m_bgmStream && g_dllExports.AIL_set_stream_volume) {
        g_dllExports.AIL_set_stream_volume(m_bgmStream, m_bgmVolume);
    }
}

int CAudio::GetBgmVolume() const
{
    return m_bgmVolume;
}

void CAudio::SetBgmPaused(bool paused)
{
    m_bgmPaused = paused;
    if (m_bgmStream && g_dllExports.AIL_pause_stream) {
        g_dllExports.AIL_pause_stream(m_bgmStream, paused ? 1 : 0);
    }
}

bool CAudio::IsBgmPaused() const
{
    return m_bgmPaused;
}

CWave::CWave() : m_fileImage(nullptr), m_isDecompress(0) {}
CWave::~CWave() { Reset(); }
bool CWave::Load(const char* fName) { return true; }
CRes* CWave::Clone() { return new CWave(); }
void CWave::Reset() {}
