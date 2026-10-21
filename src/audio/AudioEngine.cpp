#include "audio/AudioEngine.h"
#include "audio/Synth.h"
#include "audio/Wav.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace audio
{

    namespace
    {
        constexpr int kMaxSources = 32;
    }

    AudioEngine::AudioEngine()
    {
        m_device = alcOpenDevice(nullptr);
        if (!m_device)
        {
            spdlog::warn("No audio device found; sound is disabled");
            return;
        }
        m_context = alcCreateContext(m_device, nullptr);
        if (!m_context || !alcMakeContextCurrent(m_context))
        {
            spdlog::warn("Could not create an OpenAL context; sound is disabled");
            if (m_context)
                alcDestroyContext(m_context);
            alcCloseDevice(m_device);
            m_context = nullptr;
            m_device = nullptr;
            return;
        }

        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

        auto sounds = SynthesizeAll();
        // assets/sounds/<name>.wav replaces the synthesized sound of the same name (or adds a new one).
        const std::filesystem::path soundDir = std::filesystem::path(ASSETS_DIR) / "sounds";
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator(soundDir, ec))
        {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch)
                           { return static_cast<char>(std::tolower(ch)); });
            if (!entry.is_regular_file() || ext != ".wav")
                continue;
            PcmSound pcm;
            std::string error;
            if (LoadWav(entry.path().string(), pcm, error))
            {
                spdlog::info("Sound '{}' loaded from {}", entry.path().stem().string(), entry.path().filename().string());
                sounds[entry.path().stem().string()] = std::move(pcm);
            }
            else
                spdlog::warn("Sound file {} skipped: {}", entry.path().filename().string(), error);
        }

        for (const auto &[name, pcm] : sounds)
        {
            ALuint buffer = 0;
            alGenBuffers(1, &buffer);
            alBufferData(buffer, AL_FORMAT_MONO16, pcm.samples.data(), static_cast<ALsizei>(pcm.samples.size() * sizeof(int16_t)),
                         pcm.sampleRate);
            m_buffers[name] = buffer;
        }

        alGetError();
        for (int i = 0; i < kMaxSources; ++i)
        {
            ALuint source = 0;
            alGenSources(1, &source);
            if (alGetError() != AL_NO_ERROR)
                break;
            m_all.push_back(source);
            m_free.push_back(source);
        }

        m_ok = true;

        if (alcIsExtensionPresent(m_device, "ALC_EXT_EFX"))
        {
            auto genFilters = reinterpret_cast<LPALGENFILTERS>(alGetProcAddress("alGenFilters"));
            m_alFilteri = reinterpret_cast<LPALFILTERI>(alGetProcAddress("alFilteri"));
            m_alFilterf = reinterpret_cast<LPALFILTERF>(alGetProcAddress("alFilterf"));
            if (genFilters && m_alFilteri && m_alFilterf)
            {
                alGetError();
                genFilters(1, &m_lowPass);
                if (alGetError() != AL_NO_ERROR)
                    m_lowPass = 0;
                else
                    m_alFilteri(m_lowPass, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
            }
        }
        if (!m_lowPass)
            spdlog::warn("OpenAL EFX is unavailable; underwater muffling will only lower the volume");
        spdlog::info("Audio: {} sounds, {} sources ({})", m_buffers.size(), m_all.size(), alcGetString(m_device, ALC_DEVICE_SPECIFIER));
    }

    AudioEngine::~AudioEngine()
    {
        if (!m_device)
            return;
        for (ALuint s : m_all)
        {
            alSourceStop(s);
            alDeleteSources(1, &s);
        }
        for (auto &[name, b] : m_buffers)
            alDeleteBuffers(1, &b);
        if (m_lowPass)
            if (auto del = reinterpret_cast<LPALDELETEFILTERS>(alGetProcAddress("alDeleteFilters")))
                del(1, &m_lowPass);
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_context);
        alcCloseDevice(m_device);
    }

    void AudioEngine::SetListener(const glm::vec3 &position, const glm::vec3 &forward, const glm::vec3 &up)
    {
        if (!m_ok)
            return;
        alListener3f(AL_POSITION, position.x, position.y, position.z);
        const float orientation[6] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
        alListenerfv(AL_ORIENTATION, orientation);
    }

    void AudioEngine::SetMuffle(float amount)
    {
        amount = std::clamp(amount, 0.0f, 1.0f);
        if (!m_ok || std::abs(amount - m_muffle) < 0.01f)
            return;
        m_muffle = amount;
        if (!m_lowPass)
        {
            alListenerf(AL_GAIN, 1.0f - 0.5f * amount);
            return;
        }
        // Keep the lows, strip the highs; the overall level drops a little as well.
        m_alFilterf(m_lowPass, AL_LOWPASS_GAIN, 1.0f - 0.25f * amount);
        m_alFilterf(m_lowPass, AL_LOWPASS_GAINHF, 1.0f - 0.95f * amount);
        for (ALuint s : m_all)
            alSourcei(s, AL_DIRECT_FILTER, static_cast<ALint>(m_lowPass)); // re-attach so the new values take effect
    }

    void AudioEngine::SetMasterVolume(float volume)
    {
        if (m_ok)
            alListenerf(AL_GAIN, volume);
    }

    void AudioEngine::ResetSource(ALuint s) const
    {
        alSourceStop(s);
        alSourcei(s, AL_BUFFER, 0);
        alSourcei(s, AL_LOOPING, AL_FALSE);
        alSourcei(s, AL_SOURCE_RELATIVE, AL_FALSE);
        alSourcef(s, AL_GAIN, 1.0f);
        alSourcef(s, AL_PITCH, 1.0f);
        alSourcef(s, AL_REFERENCE_DISTANCE, 4.0f);
        alSourcef(s, AL_MAX_DISTANCE, 80.0f);
        alSourcef(s, AL_ROLLOFF_FACTOR, 1.5f);
        alSourcei(s, AL_DIRECT_FILTER, m_muffle > 0.0f ? static_cast<ALint>(m_lowPass) : AL_FILTER_NULL);
        alSource3f(s, AL_POSITION, 0.0f, 0.0f, 0.0f);
    }

    ALuint AudioEngine::AcquireSource()
    {
        if (!m_ok)
            return 0;
        if (m_free.empty())
            return AcquireForOneShot(); // steals the oldest one-shot if any
        const ALuint s = m_free.back();
        m_free.pop_back();
        ResetSource(s);
        return s;
    }

    ALuint AudioEngine::AcquireForOneShot()
    {
        if (!m_free.empty())
        {
            const ALuint s = m_free.back();
            m_free.pop_back();
            ResetSource(s);
            return s;
        }
        if (m_oneShots.empty())
            return 0;
        const ALuint s = m_oneShots.front();
        m_oneShots.erase(m_oneShots.begin());
        ResetSource(s);
        return s;
    }

    void AudioEngine::ReleaseSource(ALuint source)
    {
        if (!m_ok || source == 0)
            return;
        alSourceStop(source);
        m_free.push_back(source);
    }

    ALuint AudioEngine::Buffer(const std::string &name) const
    {
        auto it = m_buffers.find(name);
        return it == m_buffers.end() ? 0 : it->second;
    }

    void AudioEngine::Update()
    {
        if (!m_ok)
            return;
        for (size_t i = 0; i < m_oneShots.size();)
        {
            ALint state = 0;
            alGetSourcei(m_oneShots[i], AL_SOURCE_STATE, &state);
            if (state == AL_PLAYING)
            {
                ++i;
            }
            else
            {
                m_free.push_back(m_oneShots[i]);
                m_oneShots.erase(m_oneShots.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }
    }

    void AudioEngine::PlayOneShot(const std::string &name, const glm::vec3 &position, float volume, float pitch)
    {
        const ALuint buffer = Buffer(name);
        if (!m_ok || buffer == 0)
            return;
        const ALuint s = AcquireForOneShot();
        if (s == 0)
            return;
        alSourcei(s, AL_BUFFER, static_cast<ALint>(buffer));
        alSource3f(s, AL_POSITION, position.x, position.y, position.z);
        alSourcef(s, AL_GAIN, volume);
        alSourcef(s, AL_PITCH, pitch);
        alSourcePlay(s);
        m_oneShots.push_back(s);
    }

    void AudioEngine::Play2D(const std::string &name, float volume, float pitch)
    {
        const ALuint buffer = Buffer(name);
        if (!m_ok || buffer == 0)
            return;
        const ALuint s = AcquireForOneShot();
        if (s == 0)
            return;
        alSourcei(s, AL_BUFFER, static_cast<ALint>(buffer));
        alSourcei(s, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(s, AL_ROLLOFF_FACTOR, 0.0f);
        alSourcef(s, AL_GAIN, volume);
        alSourcef(s, AL_PITCH, pitch);
        alSourcePlay(s);
        m_oneShots.push_back(s);
    }

} // namespace audio
