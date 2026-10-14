#include "eng/AudioSource.h"
#include "eng/GameObject.h"
#include "eng/World.h"

namespace eng
{

    void AudioSource::OnAttach()
    {
        audio::AudioEngine *audio = gameObject->GetWorld().audio;
        if (!audio || !audio->Ok())
            return;

        const ALuint buffer = audio->Buffer(m_sound);
        m_source = audio->AcquireSource();
        if (m_source == 0 || buffer == 0)
            return;

        alSourcei(m_source, AL_BUFFER, static_cast<ALint>(buffer));
        alSourcei(m_source, AL_LOOPING, m_loop ? AL_TRUE : AL_FALSE);
        alSourcef(m_source, AL_GAIN, m_volume);
        if (m_spatial)
        {
            const glm::vec3 p = gameObject->GetTransform().position;
            alSource3f(m_source, AL_POSITION, p.x, p.y, p.z);
            alSourcef(m_source, AL_REFERENCE_DISTANCE, m_reference);
            alSourcef(m_source, AL_MAX_DISTANCE, m_max);
        }
        else
        {
            alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);
            alSourcef(m_source, AL_ROLLOFF_FACTOR, 0.0f);
        }
        if (m_loop && autoPlay)
            alSourcePlay(m_source);
    }

    void AudioSource::OnUpdate(float)
    {
        if (m_source == 0 || !m_spatial)
            return;
        const glm::vec3 p = gameObject->GetTransform().position;
        alSource3f(m_source, AL_POSITION, p.x, p.y, p.z);
    }

    void AudioSource::OnDestroy()
    {
        if (m_source == 0)
            return;
        if (audio::AudioEngine *audio = gameObject->GetWorld().audio)
            audio->ReleaseSource(m_source);
        m_source = 0;
    }

    void AudioSource::Play()
    {
        if (m_source != 0)
            alSourcePlay(m_source);
    }

    void AudioSource::Stop()
    {
        if (m_source != 0)
            alSourceStop(m_source);
    }

    bool AudioSource::IsPlaying() const
    {
        if (m_source == 0)
            return false;
        ALint state = 0;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }

    void AudioSource::SetVolume(float volume)
    {
        m_volume = volume;
        if (m_source != 0)
            alSourcef(m_source, AL_GAIN, volume);
    }

    void AudioSource::SetPitch(float pitch)
    {
        if (m_source != 0)
            alSourcef(m_source, AL_PITCH, pitch);
    }

} // namespace eng
