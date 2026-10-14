#include "audio/Wav.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace audio
{

    namespace
    {
        uint32_t U32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
        uint16_t U16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
    } // namespace

    bool LoadWav(const std::string &path, PcmSound &out, std::string &error)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            error = "cannot open file";
            return false;
        }
        const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) != 0 || std::memcmp(data.data() + 8, "WAVE", 4) != 0)
        {
            error = "not a RIFF/WAVE file";
            return false;
        }

        uint16_t format = 0, channels = 0, bits = 0;
        uint32_t rate = 0;
        const uint8_t *pcm = nullptr;
        size_t pcmSize = 0;

        for (size_t pos = 12; pos + 8 <= data.size();)
        {
            const uint32_t size = U32(&data[pos + 4]);
            const uint8_t *body = &data[pos + 8];
            const size_t avail = std::min<size_t>(size, data.size() - (pos + 8));
            if (std::memcmp(&data[pos], "fmt ", 4) == 0 && avail >= 16)
            {
                format = U16(body);
                channels = U16(body + 2);
                rate = U32(body + 4);
                bits = U16(body + 14);
                if (format == 0xFFFE && avail >= 26) // WAVE_FORMAT_EXTENSIBLE: real format is the sub-format GUID's first word
                    format = U16(body + 24);
            }
            else if (std::memcmp(&data[pos], "data", 4) == 0)
            {
                pcm = body;
                pcmSize = avail;
            }
            pos += 8 + size + (size & 1);
        }

        if (!pcm || channels == 0 || rate == 0)
        {
            error = "missing fmt or data chunk";
            return false;
        }
        const bool isFloat = format == 3 && bits == 32;
        if (!(format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) && !isFloat)
        {
            error = "unsupported encoding (use PCM 8/16/24/32-bit or 32-bit float)";
            return false;
        }

        const size_t bytes = bits / 8;
        const size_t frames = pcmSize / (bytes * channels);
        out.sampleRate = static_cast<int>(rate);
        out.samples.resize(frames);

        for (size_t f = 0; f < frames; ++f)
        {
            float sum = 0.0f;
            for (uint16_t c = 0; c < channels; ++c)
            {
                const uint8_t *p = pcm + (f * channels + c) * bytes;
                float v;
                if (isFloat)
                {
                    float x;
                    std::memcpy(&x, p, 4);
                    v = x;
                }
                else if (bits == 8)
                    v = (static_cast<int>(p[0]) - 128) / 128.0f;
                else if (bits == 16)
                    v = static_cast<int16_t>(U16(p)) / 32768.0f;
                else if (bits == 24)
                    v = (static_cast<int32_t>((p[0] << 8) | (p[1] << 16) | (static_cast<uint32_t>(p[2]) << 24)) >> 8) / 8388608.0f;
                else
                    v = static_cast<int32_t>(U32(p)) / 2147483648.0f;
                sum += v;
            }
            const float mono = std::clamp(sum / channels, -1.0f, 1.0f);
            out.samples[f] = static_cast<int16_t>(mono * 32767.0f);
        }
        return !out.samples.empty();
    }

} // namespace audio
