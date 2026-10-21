#pragma once
#include <glm/glm.hpp>
#include <random>

namespace eng
{

    enum class WeatherType
    {
        Sunny,
        Cloudy,
        LightRain,
        Storm,
        Count
    };

    const char *WeatherName(WeatherType type);

    // Everything the weather changes, all 0..1 except `wind` (a multiplier on the base wind).
    struct WeatherProfile
    {
        float cloud = 0.0f; // sky cover
        float dark = 0.0f;  // how grey/heavy the clouds are
        float rain = 0.0f;  // rain intensity of the rain cell
        float wind = 1.0f;  // multiplier on the world wind
        float light = 1.0f; // multiplier on sun and moon light (clouds block direct light)
        float lightning = 0.0f; // 1 = lightning strikes happen
    };

    WeatherProfile ProfileFor(WeatherType type);

    struct LightningStrike
    {
        float azimuth = 0.0f;  // radians around the camera
        float distance = 0.0f; // metres
    };

    // Runs through the weather types, each lasting a set duration, and blends smoothly between them.
    // Automatic mode picks the next type from a weighted table; Set() forces one (e.g. from a key or script).
    class WeatherSystem
    {
    public:
        WeatherSystem();

        bool automatic = true;
        float transitionSeconds = 8.0f;
        // Duration range in seconds for each type, picked at random when it starts.
        glm::vec2 duration[static_cast<int>(WeatherType::Count)] = {{70.0f, 130.0f}, {50.0f, 90.0f}, {60.0f, 110.0f}, {45.0f, 80.0f}};

        void Update(float dt);
        // Switches to `type` now. A duration <= 0 uses a random one from the table.
        void Set(WeatherType type, float durationSeconds = 0.0f);
        // Steps to the next type in Sunny -> Cloudy -> LightRain -> Storm order (manual control).
        void Cycle() { Set(static_cast<WeatherType>((static_cast<int>(m_current) + 1) % static_cast<int>(WeatherType::Count))); }

        WeatherType Current() const { return m_current; }
        float TimeRemaining() const { return m_remaining; }
        // Current profile, blended across the transition.
        const WeatherProfile &Blend() const { return m_blend; }
        // 0..1 progress through the blend (1 = fully the current type).
        float TransitionProgress() const { return m_blendT; }

        // Lightning brightness 0..1 for this frame (a short multi-pulse flicker after each strike).
        float Flash() const { return m_flash; }
        // Pops a strike that just happened (at most one per frame).
        bool TakeStrike(LightningStrike &out);
        // Pops a thunderclap whose delay has elapsed; volume falls with strike distance.
        bool TakeThunder(float &volume);

    private:
        WeatherType PickNext();
        float RandomDuration(WeatherType type);
        float Rand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(m_rng); }

        std::mt19937 m_rng;
        WeatherType m_current = WeatherType::LightRain;
        WeatherProfile m_from, m_blend;
        float m_blendT = 1.0f;
        float m_remaining = 0.0f;

        float m_nextStrike = 6.0f;
        float m_flashTime = 99.0f;
        float m_flash = 0.0f;
        bool m_strikePending = false;
        LightningStrike m_strike;
        float m_thunderDelay = -1.0f;
        float m_thunderVolume = 0.0f;
    };

} // namespace eng
