#pragma once
#include <cmath>
#include <glm/glm.hpp>

namespace eng
{

    // Clock for the day/night cycle. Hours run 0-24; 6 = sunrise, 12 = noon, 18 = sunset, 0 = midnight.
    struct DayNightCycle
    {
        bool enabled = true;
        bool paused = false;
        float timeOfDay = 20.0f;
        float dayLengthSeconds = 180.0f; // real seconds per full 24h

        void SetTime(float hours)
        {
            timeOfDay = std::fmod(hours, 24.0f);
            if (timeOfDay < 0.0f)
                timeOfDay += 24.0f;
        }

        void Advance(float dt)
        {
            if (enabled && !paused)
                SetTime(timeOfDay + dt * 24.0f / dayLengthSeconds);
        }

        // Unit vector pointing from the world towards the sun.
        glm::vec3 SunDirection() const
        {
            const float a = (timeOfDay - 6.0f) / 24.0f * 6.28318530718f;
            return glm::normalize(glm::vec3(std::cos(a), std::sin(a), 0.35f));
        }

        float Daylight() const
        {
            const float t = glm::clamp((SunDirection().y + 0.12f) / 0.4f, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
    };

} // namespace eng
