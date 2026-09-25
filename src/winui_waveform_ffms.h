#pragma once

#include <libaegisub/fs.h>

#include <cstddef>
#include <string>

bool WriteWinUiWaveform(
    agi::fs::path const& input,
    agi::fs::path const& output,
    size_t bins,
    std::string& error);
