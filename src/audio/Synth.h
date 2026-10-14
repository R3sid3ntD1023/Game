#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace audio {

struct PcmSound {
    std::vector<int16_t> samples; // mono, signed 16-bit
    int sampleRate = 44100;
};

// Builds every game sound procedurally (noise, filters, oscillators), so no audio assets are needed.
// Names: rain_loop, wind_loop, footstep_0..3, gunshot_pistol, gunshot_rifle, reload, dry_fire, flesh_hit,
// pickup, heal, door, flashlight, raptor_roar.
std::unordered_map<std::string, PcmSound> SynthesizeAll();

} // namespace audio
