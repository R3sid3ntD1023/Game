#include "audio/Synth.h"
#include <algorithm>
#include <cmath>

namespace audio
{

    namespace
    {

        constexpr int kRate = 44100;
        constexpr float kPi = 3.14159265f;
        constexpr float kTwoPi = 6.2831853f;

        struct Noise
        {
            uint32_t s;
            explicit Noise(uint32_t seed) : s(seed * 2654435761u + 12345u) {}
            float Next()
            { // -1..1
                s ^= s << 13;
                s ^= s >> 17;
                s ^= s << 5;
                return static_cast<float>(s) / 2147483648.0f - 1.0f;
            }
        };

        struct LowPass
        {
            float y = 0.0f;
            float a;
            explicit LowPass(float cutoff) { SetCutoff(cutoff); }
            void SetCutoff(float cutoff) { a = 1.0f - std::exp(-kTwoPi * cutoff / static_cast<float>(kRate)); }
            float Process(float x) { return y += a * (x - y); }
        };

        int Count(float seconds) { return static_cast<int>(seconds * static_cast<float>(kRate)); }
        float Time(int i) { return static_cast<float>(i) / static_cast<float>(kRate); }

        PcmSound ToPcm(const std::vector<float> &f)
        {
            PcmSound p;
            p.sampleRate = kRate;
            p.samples.resize(f.size());
            for (size_t i = 0; i < f.size(); ++i)
                p.samples[i] = static_cast<int16_t>(std::clamp(f[i], -1.0f, 1.0f) * 32767.0f);
            return p;
        }

        // Crossfades the last `fade` samples into the first ones so the result loops without a click.
        std::vector<float> MakeLoop(const std::vector<float> &x, int fade)
        {
            const size_t n = x.size() - static_cast<size_t>(fade);
            std::vector<float> out(x.begin(), x.begin() + static_cast<std::ptrdiff_t>(n));
            for (int i = 0; i < fade; ++i)
            {
                const float w = static_cast<float>(i) / static_cast<float>(fade);
                out[static_cast<size_t>(i)] = x[static_cast<size_t>(i)] * w + x[n + static_cast<size_t>(i)] * (1.0f - w);
            }
            return out;
        }

        // Short metallic tick starting at `start` seconds.
        float Click(float t, float start, float freq, float decay)
        {
            const float u = t - start;
            if (u < 0.0f)
                return 0.0f;
            return (std::sin(kTwoPi * freq * u) + 0.5f * std::sin(kTwoPi * freq * 1.5f * u)) * std::exp(-u * decay) * 0.5f;
        }

        PcmSound Rain()
        {
            const int n = Count(6.0f), fade = Count(0.5f);
            Noise noise(1);
            LowPass high(7000.0f), low(500.0f);
            std::vector<float> x(static_cast<size_t>(n + fade));
            float dripPhase = 0.0f, dripFreq = 0.0f, dripAmp = 0.0f;
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float r = noise.Next();
                const float band = high.Process(r) - low.Process(r); // hiss between 500 Hz and 7 kHz
                if (noise.Next() > 0.9985f)
                {
                    dripFreq = 2500.0f + 2500.0f * (noise.Next() * 0.5f + 0.5f);
                    dripAmp = 0.12f;
                    dripPhase = 0.0f;
                }
                const float drip = std::sin(dripPhase) * dripAmp;
                dripPhase += kTwoPi * dripFreq / static_cast<float>(kRate);
                dripAmp *= 0.997f;
                x[i] = band * 0.38f + drip;
            }
            return ToPcm(MakeLoop(x, fade));
        }

        PcmSound Wind()
        {
            const float period = 10.0f; // gust modulation repeats every `period`, so the crossfaded loop is seamless
            const int n = Count(period), fade = Count(0.6f);
            Noise noise(2);
            LowPass lp1(300.0f), lp2(300.0f);
            std::vector<float> x(static_cast<size_t>(n + fade));
            float whistlePhase = 0.0f;
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                const float g1 = 0.5f + 0.5f * std::sin(kTwoPi * 1.0f * t / period + 0.6f);
                const float g2 = 0.5f + 0.5f * std::sin(kTwoPi * 3.0f * t / period + 1.9f);
                const float gust = 0.15f + 0.85f * g1 * g2;

                lp1.SetCutoff(180.0f + 520.0f * gust);
                lp2.SetCutoff(260.0f + 700.0f * gust);
                const float body = lp2.Process(lp1.Process(noise.Next())) * 2.5f * (0.25f + 0.75f * gust);
                const float whistle = std::sin(whistlePhase) * 0.03f * gust * gust;
                whistlePhase += kTwoPi * (420.0f + 120.0f * g1) / static_cast<float>(kRate);
                x[i] = body + whistle;
            }
            return ToPcm(MakeLoop(x, fade));
        }

        PcmSound Footstep(int variant)
        {
            const int n = Count(0.28f);
            Noise noise(static_cast<uint32_t>(10 + variant));
            LowPass body(700.0f + 150.0f * static_cast<float>(variant)), squelchLp(3000.0f);
            const float f0 = 60.0f + 8.0f * static_cast<float>(variant);
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                const float thump = std::sin(kTwoPi * f0 * t) * std::exp(-t * 30.0f) * 0.7f;
                const float thud = body.Process(noise.Next()) * std::exp(-t * 30.0f) * 2.2f;
                const float r = noise.Next();
                const float squelch = (r - squelchLp.Process(r)) * std::exp(-t * 80.0f) * 0.18f; // wet high-frequency slap
                x[i] = thump + thud + squelch;
            }
            return ToPcm(x);
        }

        PcmSound Gunshot(bool rifle)
        {
            const int n = Count(rifle ? 0.9f : 1.0f);
            Noise noise(rifle ? 21 : 20);
            LowPass crackLp(8000.0f), tailLp(rifle ? 1800.0f : 1200.0f);
            float phase = 0.0f;
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                const float crack = crackLp.Process(noise.Next()) * std::exp(-t * (rifle ? 90.0f : 70.0f)) * 1.2f;
                const float f = (rifle ? 240.0f : 170.0f) * std::exp(-t * 9.0f) + 45.0f;
                phase += kTwoPi * f / static_cast<float>(kRate);
                const float boom = std::sin(phase) * std::exp(-t * (rifle ? 16.0f : 12.0f)) * 0.8f;
                const float tail = tailLp.Process(noise.Next()) * std::exp(-t * 5.0f) * 2.0f;
                x[i] = std::tanh((crack + boom + tail) * 1.3f);
            }
            return ToPcm(x);
        }

        PcmSound Reload()
        {
            const int n = Count(0.75f);
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                x[i] = Click(t, 0.0f, 1800.0f, 120.0f) + Click(t, 0.38f, 900.0f, 90.0f) * 1.2f + Click(t, 0.52f, 2600.0f, 160.0f) * 0.6f;
            }
            return ToPcm(x);
        }

        PcmSound DryFire()
        {
            const int n = Count(0.12f);
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
                x[i] = Click(Time(static_cast<int>(i)), 0.0f, 2100.0f, 140.0f);
            return ToPcm(x);
        }

        PcmSound FleshHit()
        {
            const int n = Count(0.3f);
            Noise noise(30);
            LowPass lp(900.0f);
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                x[i] = std::sin(kTwoPi * 85.0f * t) * std::exp(-t * 20.0f) * 0.8f + lp.Process(noise.Next()) * std::exp(-t * 25.0f) * 2.0f;
            }
            return ToPcm(x);
        }

        PcmSound Pickup()
        {
            const int n = Count(0.25f);
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                const float a = t < 0.08f ? std::sin(kTwoPi * 660.0f * t) * std::exp(-t * 25.0f) : 0.0f;
                const float b = t >= 0.08f ? std::sin(kTwoPi * 990.0f * (t - 0.08f)) * std::exp(-(t - 0.08f) * 18.0f) : 0.0f;
                x[i] = (a + b) * 0.5f;
            }
            return ToPcm(x);
        }

        PcmSound Heal()
        {
            const int n = Count(0.6f);
            const float notes[3] = {440.0f, 554.4f, 659.3f};
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                float s = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    const float u = t - 0.12f * static_cast<float>(k);
                    if (u >= 0.0f)
                        s += std::sin(kTwoPi * notes[k] * u) * std::exp(-u * 7.0f);
                }
                x[i] = s * 0.3f;
            }
            return ToPcm(x);
        }

        PcmSound Door()
        {
            const int n = Count(1.0f);
            Noise noise(40);
            LowPass lp(350.0f);
            float creakPhase = 0.0f;
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                const float env = std::sin(kPi * t);
                lp.SetCutoff(300.0f + 200.0f * std::sin(kTwoPi * 2.0f * t));
                creakPhase += kTwoPi * (140.0f + 30.0f * std::sin(kTwoPi * 3.0f * t)) / static_cast<float>(kRate);
                x[i] = lp.Process(noise.Next()) * env * 2.0f + std::sin(creakPhase) * 0.12f * env;
            }
            return ToPcm(x);
        }

        PcmSound Flashlight()
        {
            const int n = Count(0.08f);
            Noise noise(50);
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                x[i] = std::sin(kTwoPi * 2400.0f * t) * std::exp(-t * 150.0f) * 0.6f + noise.Next() * std::exp(-t * 300.0f) * 0.3f;
            }
            return ToPcm(x);
        }

        PcmSound RaptorRoar()
        {
            const int n = Count(0.9f);
            Noise noise(60);
            LowPass lp(2500.0f);
            float phase = 0.0f;
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                const float f = 520.0f + 380.0f * std::sin(kPi * t / 0.9f) + 25.0f * std::sin(kTwoPi * 30.0f * t);
                phase += kTwoPi * f / static_cast<float>(kRate);
                const float env = std::min(1.0f, t * 30.0f) * std::exp(-t * 2.2f);
                const float tone = std::sin(phase) + 0.5f * std::sin(2.0f * phase) + 0.3f * std::sin(3.0f * phase);
                x[i] = (tone * 0.35f + lp.Process(noise.Next()) * 1.2f) * env * 0.8f;
            }
            return ToPcm(x);
        }

        // A sharp crack followed by a rolling low rumble made of several decaying swells.
        PcmSound Thunder()
        {
            const int n = Count(5.0f);
            Noise noise(77);
            LowPass rumble1(120.0f), rumble2(70.0f), crack(1800.0f);
            std::vector<float> x(static_cast<size_t>(n));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float t = Time(static_cast<int>(i));
                const float r = noise.Next();
                const float swell = 0.6f * std::exp(-t * 0.9f) + 0.4f * std::exp(-std::fmod(t, 1.1f) * 3.0f) * std::exp(-t * 0.5f);
                const float body = rumble2.Process(rumble1.Process(r)) * 9.0f * swell;
                const float snap = crack.Process(r) * std::exp(-t * 18.0f) * 1.4f;
                x[i] = (body + snap) * std::min(1.0f, t * 40.0f) * std::min(1.0f, (5.0f - t) * 2.0f);
            }
            return ToPcm(x);
        }

    } // namespace

    std::unordered_map<std::string, PcmSound> SynthesizeAll()
    {
        std::unordered_map<std::string, PcmSound> sounds;
        sounds["thunder"] = Thunder();
        sounds["rain_loop"] = Rain();
        sounds["wind_loop"] = Wind();
        for (int v = 0; v < 4; ++v)
            sounds["footstep_" + std::to_string(v)] = Footstep(v);
        sounds["gunshot_pistol"] = Gunshot(false);
        sounds["gunshot_rifle"] = Gunshot(true);
        sounds["reload"] = Reload();
        sounds["dry_fire"] = DryFire();
        sounds["flesh_hit"] = FleshHit();
        sounds["pickup"] = Pickup();
        sounds["heal"] = Heal();
        sounds["door"] = Door();
        sounds["flashlight"] = Flashlight();
        sounds["raptor_roar"] = RaptorRoar();
        return sounds;
    }

} // namespace audio
