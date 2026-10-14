#pragma once
#include "eng/Component.h"
#include <AL/al.h>
#include <string>

namespace eng {

// A sound attached to a GameObject. Spatial sources follow the object's position (local sound);
// non-spatial sources play at the listener (ambience such as rain and wind).
class AudioSource : public Component {
public:
    AudioSource(std::string sound, bool loop = false, float volume = 1.0f, bool spatial = true,
                float referenceDistance = 4.0f, float maxDistance = 80.0f)
        : m_sound(std::move(sound)), m_loop(loop), m_volume(volume), m_spatial(spatial),
          m_reference(referenceDistance), m_max(maxDistance) {}

    void OnAttach() override;
    void OnUpdate(float dt) override;
    void OnDestroy() override;

    void Play();
    void Stop();
    bool IsPlaying() const;
    void SetVolume(float volume);
    void SetPitch(float pitch);

    bool autoPlay = true; // looping sources start on attach

private:
    std::string m_sound;
    bool m_loop;
    float m_volume;
    bool m_spatial;
    float m_reference;
    float m_max;
    ALuint m_source = 0;
};

} // namespace eng
