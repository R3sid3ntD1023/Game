#include "eng/Weather.h"
#include <algorithm>
#include <cmath>

namespace eng
{

    const char *WeatherName(WeatherType type)
    {
        switch (type)
        {
        case WeatherType::Sunny:
            return "Sunny";
        case WeatherType::Cloudy:
            return "Cloudy";
        case WeatherType::LightRain:
            return "Light rain";
        case WeatherType::Storm:
            return "Storm";
        default:
            return "?";
        }
    }

    WeatherProfile ProfileFor(WeatherType type)
    {
        //                          cloud dark rain  wind light lightning
        switch (type)
        {
        case WeatherType::Sunny:
            return {0.08f, 0.0f, 0.0f, 0.6f, 1.0f, 0.0f};
        case WeatherType::Cloudy:
            return {0.7f, 0.25f, 0.0f, 0.9f, 0.45f, 0.0f};
        case WeatherType::LightRain:
            return {0.85f, 0.5f, 0.3f, 1.1f, 0.3f, 0.0f};
        case WeatherType::Storm:
            return {1.0f, 1.0f, 1.0f, 1.8f, 0.12f, 1.0f};
        default:
            return {};
        }
    }

    namespace
    {
        WeatherProfile Mix(const WeatherProfile &a, const WeatherProfile &b, float t)
        {
            auto m = [t](float x, float y) { return x + (y - x) * t; };
            return {m(a.cloud, b.cloud), m(a.dark, b.dark), m(a.rain, b.rain), m(a.wind, b.wind), m(a.light, b.light), m(a.lightning, b.lightning)};
        }
    } // namespace

    WeatherSystem::WeatherSystem() : m_rng(20261021u)
    {
        m_blend = m_from = ProfileFor(m_current);
        m_remaining = RandomDuration(m_current);
    }

    float WeatherSystem::RandomDuration(WeatherType type)
    {
        const glm::vec2 d = duration[static_cast<int>(type)];
        return Rand(d.x, std::max(d.x, d.y));
    }

    WeatherType WeatherSystem::PickNext()
    {
        // Rows: from; columns: Sunny, Cloudy, LightRain, Storm. Weather builds up and clears gradually.
        static constexpr float kWeights[4][4] = {
            {0.0f, 0.8f, 0.2f, 0.0f},
            {0.35f, 0.0f, 0.4f, 0.25f},
            {0.2f, 0.5f, 0.0f, 0.3f},
            {0.0f, 0.4f, 0.6f, 0.0f},
        };
        const float *w = kWeights[static_cast<int>(m_current)];
        float r = Rand(0.0f, 1.0f);
        for (int i = 0; i < 4; ++i)
        {
            if (r < w[i])
                return static_cast<WeatherType>(i);
            r -= w[i];
        }
        return WeatherType::Cloudy;
    }

    void WeatherSystem::Set(WeatherType type, float durationSeconds)
    {
        m_from = m_blend;
        m_current = type;
        m_blendT = 0.0f;
        m_remaining = durationSeconds > 0.0f ? durationSeconds : RandomDuration(type);
        m_nextStrike = Rand(2.0f, 6.0f);
    }

    void WeatherSystem::Update(float dt)
    {
        if (automatic)
        {
            m_remaining -= dt;
            if (m_remaining <= 0.0f)
                Set(PickNext());
        }

        if (m_blendT < 1.0f)
        {
            m_blendT = std::min(1.0f, m_blendT + dt / std::max(transitionSeconds, 0.01f));
            const float s = m_blendT * m_blendT * (3.0f - 2.0f * m_blendT);
            m_blend = Mix(m_from, ProfileFor(m_current), s);
        }

        // Lightning: random strikes while the storm is established.
        if (m_blend.lightning > 0.6f)
        {
            m_nextStrike -= dt;
            if (m_nextStrike <= 0.0f)
            {
                m_nextStrike = Rand(4.0f, 13.0f);
                m_strike = {Rand(0.0f, 6.2831853f), Rand(80.0f, 400.0f)};
                m_strikePending = true;
                m_flashTime = 0.0f;
                m_thunderDelay = m_strike.distance / 340.0f; // speed of sound
                m_thunderVolume = std::clamp(1.3f - m_strike.distance / 450.0f, 0.3f, 1.0f);
            }
        }

        // Two quick pulses then a longer one, each decaying fast.
        m_flashTime += dt;
        auto pulse = [this](float start, float amp) { return m_flashTime < start ? 0.0f : amp * std::exp(-(m_flashTime - start) * 9.0f); };
        m_flash = std::clamp(std::max({pulse(0.0f, 1.0f), pulse(0.13f, 0.55f), pulse(0.3f, 0.85f)}), 0.0f, 1.0f);

        if (m_thunderDelay >= 0.0f)
            m_thunderDelay -= dt;
    }

    bool WeatherSystem::TakeStrike(LightningStrike &out)
    {
        if (!m_strikePending)
            return false;
        m_strikePending = false;
        out = m_strike;
        return true;
    }

    bool WeatherSystem::TakeThunder(float &volume)
    {
        if (m_thunderDelay >= -0.5f && m_thunderDelay < 0.0f && m_thunderVolume > 0.0f)
        {
            volume = m_thunderVolume;
            m_thunderVolume = 0.0f;
            m_thunderDelay = -1.0f;
            return true;
        }
        return false;
    }

} // namespace eng
