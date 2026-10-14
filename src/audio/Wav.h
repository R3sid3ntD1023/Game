#pragma once
#include "audio/Synth.h"
#include <string>

namespace audio {

// Reads a RIFF/WAVE file (PCM 8/16/24/32-bit or 32-bit float, any channel count; mixed down to mono 16-bit).
// Returns false and sets `error` on failure.
bool LoadWav(const std::string& path, PcmSound& out, std::string& error);

} // namespace audio
