#pragma once
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace audio {

// OpenAL device, context, sound buffers and a pool of sources. Sounds are synthesized by default;
// any assets/sounds/<name>.wav replaces the sound with that name.
// If no audio device is available every call becomes a no-op.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool Ok() const { return m_ok; }

    void SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);
    void SetMasterVolume(float volume);
    // Muffles every sound with a low-pass filter: 0 = clear, 1 = fully muffled (underwater). Needs ALC_EXT_EFX.
    void SetMuffle(float amount);
    void Update(); // recycles finished one-shot sources; call once per frame

    // Fire-and-forget sounds. Play2D is not positional (weapon, UI, footsteps of the listener).
    void PlayOneShot(const std::string& name, const glm::vec3& position, float volume = 1.0f, float pitch = 1.0f);
    void Play2D(const std::string& name, float volume = 1.0f, float pitch = 1.0f);

    // For AudioSource components that own a source for their lifetime.
    ALuint AcquireSource();
    void ReleaseSource(ALuint source);
    ALuint Buffer(const std::string& name) const;

private:
    void ResetSource(ALuint source) const;
    ALuint AcquireForOneShot();

    bool m_ok = false;
    ALCdevice* m_device = nullptr;
    ALCcontext* m_context = nullptr;
    std::unordered_map<std::string, ALuint> m_buffers;
    std::vector<ALuint> m_free;
    std::vector<ALuint> m_oneShots; // oldest first
    std::vector<ALuint> m_all;

    ALuint m_lowPass = 0; // EFX filter object, 0 when EFX is unavailable
    float m_muffle = 0.0f;
    LPALFILTERI m_alFilteri = nullptr;
    LPALFILTERF m_alFilterf = nullptr;
};

} // namespace audio
