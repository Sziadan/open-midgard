#include "Audio.h"
#include "core/DllMgr.h"
#include "core/File.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>

#include <algorithm>

namespace {
constexpr int kDefaultDigitalRate = 22050;
constexpr int kDefaultDigitalBits = 16;
constexpr int kDefaultDigitalChannels = 1;
constexpr int kMaxManagedSamples = 24;
constexpr int kMaxVolume = 127;
constexpr int kDefaultSoundMaxDist = 250;
constexpr int kDefaultSoundMinDist = 40;

int ClampMilesVolume(int value)
{
    return (std::max)(0, (std::min)(kMaxVolume, value));
}

float ClampUnit(float value)
{
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

bool CopyBuffer(unsigned char** dst, int* dstSize, const unsigned char* src, int size)
{
    if (!dst || !dstSize || !src || size <= 0) {
        return false;
    }

    unsigned char* bytes = new unsigned char[size];
    std::memcpy(bytes, src, static_cast<size_t>(size));
    *dst = bytes;
    *dstSize = size;
    return true;
}
}

CAudio::CAudio()
    : m_digDriver(nullptr),
      m_soundVolume(1.0f),
      m_bgmStream(nullptr),
      m_bgmVolume(127),
      m_bgmPaused(false),
      m_bgmPendingStart(false),
      m_startedUp(false)
{
}

CAudio::~CAudio()
{
    Shutdown();
}

CAudio* CAudio::GetInstance() {
    static CAudio instance;
    return &instance;
}

bool CAudio::Init() {
    if (m_digDriver) {
        return true;
    }

    if (!g_dllExports.AIL_startup || !g_dllExports.AIL_open_digital_driver) {
        return false;
    }

    if (!m_startedUp) {
        if (g_dllExports.AIL_set_redist_directory) {
            g_dllExports.AIL_set_redist_directory(".\\");
        }
        if (g_dllExports.AIL_set_preference) {
            g_dllExports.AIL_set_preference(15u, 0);
        }
        g_dllExports.AIL_startup();
        m_startedUp = true;
    }

    m_digDriver = g_dllExports.AIL_open_digital_driver(kDefaultDigitalRate, kDefaultDigitalBits, kDefaultDigitalChannels, 1);
    if (!m_digDriver) {
        return false;
    }

    m_samples.clear();
    if (g_dllExports.AIL_allocate_sample_handle) {
        m_samples.reserve(kMaxManagedSamples);
        for (int index = 0; index < kMaxManagedSamples; ++index) {
            HSAMPLE sample = g_dllExports.AIL_allocate_sample_handle(m_digDriver);
            if (!sample) {
                break;
            }
            m_samples.push_back(sample);
        }
    }
    return m_digDriver != nullptr;
}

void CAudio::Shutdown()
{
    StopAllSounds();
    StopBGM();

    if (g_dllExports.AIL_release_sample_handle) {
        for (HSAMPLE sample : m_samples) {
            if (sample) {
                g_dllExports.AIL_release_sample_handle(sample);
            }
        }
    }
    m_samples.clear();

    if (m_digDriver && g_dllExports.AIL_close_digital_driver) {
        g_dllExports.AIL_close_digital_driver(m_digDriver);
    }
    m_digDriver = nullptr;

    if (m_startedUp && g_dllExports.AIL_shutdown) {
        g_dllExports.AIL_shutdown();
    }
    m_startedUp = false;
}

HSAMPLE CAudio::FindReusableSample()
{
    if (!g_dllExports.AIL_sample_status) {
        return nullptr;
    }

    for (HSAMPLE sample : m_samples) {
        if (!sample) {
            continue;
        }
        if (g_dllExports.AIL_sample_status(sample) == 2u) {
            return sample;
        }
    }

    if (m_digDriver && g_dllExports.AIL_allocate_sample_handle) {
        HSAMPLE sample = g_dllExports.AIL_allocate_sample_handle(m_digDriver);
        if (sample) {
            m_samples.push_back(sample);
        }
        return sample;
    }

    return nullptr;
}

bool CAudio::PlaySound(const char* fName, float vol) {
    if (!fName || !*fName) {
        return false;
    }

    if (!Init() || !g_soundMode || !g_dllExports.AIL_init_sample || !g_dllExports.AIL_set_sample_file
        || !g_dllExports.AIL_start_sample) {
        return false;
    }

    CWave* wave = g_resMgr.GetAs<CWave>(fName);
    if (!wave || !wave->m_fileImage || wave->m_fileSize <= 0) {
        return false;
    }

    const float clampedVolume = ClampUnit(vol * m_soundVolume);
    if (clampedVolume <= 0.0f) {
        return false;
    }

    HSAMPLE sample = FindReusableSample();
    if (!sample) {
        return false;
    }

    g_dllExports.AIL_init_sample(sample);
    g_dllExports.AIL_set_sample_file(sample, wave->m_fileImage, 0);
    if (g_dllExports.AIL_set_sample_volume) {
        g_dllExports.AIL_set_sample_volume(sample, ClampMilesVolume(static_cast<int>(clampedVolume * static_cast<float>(kMaxVolume))));
    }
    g_dllExports.AIL_start_sample(sample);
    return true;
}

bool CAudio::PlaySound3D(const char* fName, const vector3d& soundPos, const vector3d& listenerPos,
    int volumeMaxDist, int volumeMinDist, float vol)
{
    const float dx = soundPos.x - listenerPos.x;
    const float dy = soundPos.y - listenerPos.y;
    const float dz = soundPos.z - listenerPos.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float maxDist = static_cast<float>((std::max)(volumeMaxDist, 1));
    const float minDist = static_cast<float>((std::max)(volumeMinDist, 1));
    if (distance >= maxDist) {
        return false;
    }

    float distanceFactor = 1.0f;
    if (distance > minDist) {
        distanceFactor = minDist / distance;
    }
    return PlaySound(fName, ClampUnit(vol * distanceFactor));
}

void CAudio::PlayBGM(const char* fName) {
    if (!fName || !*fName) {
        return;
    }

    if (!Init() || !g_dllExports.AIL_open_stream) {
        return;
    }

    const std::string normalizedPath = NormalizeAudioPath(fName);
    if (!normalizedPath.empty() && normalizedPath == m_bgmPath && m_bgmStream) {
        if (!m_bgmPaused && m_bgmPendingStart && g_dllExports.AIL_start_stream) {
            g_dllExports.AIL_start_stream(m_bgmStream);
            m_bgmPendingStart = false;
        }
        if (g_dllExports.AIL_pause_stream) {
            g_dllExports.AIL_pause_stream(m_bgmStream, m_bgmPaused ? 1 : 0);
        }
        return;
    }

    StopBGM();

    void* stream = g_dllExports.AIL_open_stream(m_digDriver, normalizedPath.c_str(), 0);
    if (stream) {
        m_bgmStream = stream;
        m_bgmPath = normalizedPath;
        m_bgmPendingStart = m_bgmPaused;
        if (g_dllExports.AIL_set_stream_loop_count) {
            g_dllExports.AIL_set_stream_loop_count(stream, 0);
        }
        if (g_dllExports.AIL_set_stream_volume) {
            g_dllExports.AIL_set_stream_volume(stream, m_bgmVolume);
        }
        if (!m_bgmPendingStart && g_dllExports.AIL_start_stream) {
            g_dllExports.AIL_start_stream(stream);
        }
        if (g_dllExports.AIL_pause_stream) {
            g_dllExports.AIL_pause_stream(stream, m_bgmPaused ? 1 : 0);
        }
    }
}

void CAudio::StopBGM() {
    if (m_bgmStream && g_dllExports.AIL_close_stream) {
        g_dllExports.AIL_close_stream(m_bgmStream);
    }
    m_bgmStream = nullptr;
    m_bgmPendingStart = false;
    m_bgmPath.clear();
}

void CAudio::StopAllSounds()
{
    if (!g_dllExports.AIL_end_sample) {
        return;
    }

    for (HSAMPLE sample : m_samples) {
        if (sample) {
            g_dllExports.AIL_end_sample(sample);
        }
    }
}

void CAudio::SetVolume(float vol) {
    m_soundVolume = ClampUnit(vol);
}

float CAudio::GetVolume() const {
    return m_soundVolume;
}

void CAudio::SetBgmVolume(int vol)
{
    m_bgmVolume = ClampMilesVolume(vol);
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
    const bool wasPaused = m_bgmPaused;
    m_bgmPaused = paused;
    if (!paused && wasPaused && m_bgmPendingStart && m_bgmStream && g_dllExports.AIL_start_stream) {
        g_dllExports.AIL_start_stream(m_bgmStream);
        m_bgmPendingStart = false;
    }
    if (m_bgmStream && g_dllExports.AIL_pause_stream) {
        g_dllExports.AIL_pause_stream(m_bgmStream, paused ? 1 : 0);
    }
}

bool CAudio::IsBgmPaused() const
{
    return m_bgmPaused;
}

const std::string& CAudio::GetCurrentBgmPath() const
{
    return m_bgmPath;
}

std::string CAudio::CanonicalizeMapName(const char* rswName)
{
    std::string name = rswName ? rswName : "";
    for (char& ch : name) {
        if (ch == '/') {
            ch = '\\';
        }
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return name;
}

std::string CAudio::NormalizeAudioPath(const char* path)
{
    std::string normalized = path ? path : "";
    for (char& ch : normalized) {
        if (ch == '/') {
            ch = '\\';
        }
    }
    return normalized;
}

bool CAudio::EnsureMapBgmTableLoaded()
{
    if (!m_mapBgmTable.empty()) {
        return true;
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData("data\\mp3nametable.txt", &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return false;
    }

    std::string content(reinterpret_cast<const char*>(bytes), static_cast<size_t>(size));
    delete[] bytes;

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        const size_t firstHash = line.find('#');
        if (firstHash == std::string::npos) {
            continue;
        }
        const size_t secondHash = line.find('#', firstHash + 1u);
        if (secondHash == std::string::npos || secondHash <= firstHash + 1u) {
            continue;
        }

        MapBgmEntry entry{};
        entry.rswName = CanonicalizeMapName(line.substr(0, firstHash).c_str());
        entry.bgmPath = NormalizeAudioPath(line.substr(firstHash + 1u, secondHash - firstHash - 1u).c_str());
        if (!entry.rswName.empty() && !entry.bgmPath.empty()) {
            m_mapBgmTable.push_back(entry);
        }
    }

    return !m_mapBgmTable.empty();
}

std::string CAudio::ResolveMapBgmPath(const char* rswName)
{
    const std::string canonical = CanonicalizeMapName(rswName);
    if (canonical.empty()) {
        return "bgm\\01.mp3";
    }

    EnsureMapBgmTableLoaded();
    for (const MapBgmEntry& entry : m_mapBgmTable) {
        if (entry.rswName == canonical) {
            return entry.bgmPath;
        }
    }
    return "bgm\\01.mp3";
}

CWave::CWave() : m_fileImage(nullptr), m_isDecompress(0), m_fileSize(0) {}
CWave::~CWave() { Reset(); }

bool CWave::Load(const char* fName)
{
    Reset();

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(fName, &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return false;
    }

    if (!CopyBuffer(&m_fileImage, &m_fileSize, bytes, size)) {
        delete[] bytes;
        return false;
    }

    delete[] bytes;
    return true;
}

CRes* CWave::Clone() { return new CWave(); }
void CWave::Reset()
{
    delete[] m_fileImage;
    m_fileImage = nullptr;
    m_fileSize = 0;
}
