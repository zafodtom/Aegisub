#include "winui_waveform_ffms.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

#ifdef WITH_FFMS2
#include <ffms.h>
#endif

bool WriteWinUiWaveform(
    agi::fs::path const& input,
    agi::fs::path const& output,
    size_t bins,
    std::string& error)
{
#ifndef WITH_FFMS2
    error = "This Aegisub build does not include FFMS2 audio decoding.";
    return false;
#else
    bins = (std::max)(size_t{ 200 }, (std::min)(bins, size_t{ 5000 }));

    char errorBuffer[2048]{};
    FFMS_ErrorInfo errorInfo{};
    errorInfo.Buffer = errorBuffer;
    errorInfo.BufferSize = sizeof(errorBuffer);
    errorInfo.ErrorType = FFMS_ERROR_SUCCESS;
    errorInfo.SubType = FFMS_ERROR_SUCCESS;

    FFMS_Init(0, 0);

    auto const inputPath = input.string();
    FFMS_Indexer* indexer = FFMS_CreateIndexer(inputPath.c_str(), &errorInfo);
    if (!indexer)
    {
        error = std::string("Audio indexing failed: ") + errorInfo.Buffer;
        return false;
    }

    int audioTrack = -1;
    auto const trackCount = FFMS_GetNumTracksI(indexer);
    for (int track = 0; track < trackCount; ++track)
    {
        if (FFMS_GetTrackTypeI(indexer, track) == FFMS_TYPE_AUDIO)
        {
            audioTrack = track;
            break;
        }
    }

    if (audioTrack < 0)
    {
        FFMS_CancelIndexing(indexer);
        error = "The selected media file has no audio track.";
        return false;
    }

    FFMS_TrackIndexSettings(indexer, audioTrack, 1, 0);
    FFMS_TrackTypeIndexSettings(indexer, FFMS_TYPE_VIDEO, 1, 0);

    FFMS_Index* index = FFMS_DoIndexing2(indexer, FFMS_IEH_CLEAR_TRACK, &errorInfo);
    if (!index)
    {
        error = std::string("Audio indexing failed: ") + errorInfo.Buffer;
        return false;
    }

    FFMS_AudioSource* audio = FFMS_CreateAudioSource(
        inputPath.c_str(),
        audioTrack,
        index,
        FFMS_DELAY_FIRST_VIDEO_TRACK,
        &errorInfo);

    if (!audio)
    {
        FFMS_DestroyIndex(index);
        error = std::string("Audio track could not be opened: ") + errorInfo.Buffer;
        return false;
    }

    auto const original = *FFMS_GetAudioProperties(audio);
    if (original.NumSamples <= 0 || original.SampleRate <= 0)
    {
        FFMS_DestroyAudioSource(audio);
        FFMS_DestroyIndex(index);
        error = "The selected audio track contains no decodable samples.";
        return false;
    }

    FFMS_ResampleOptions* resample = FFMS_CreateResampleOptions(audio);
    if (!resample)
    {
        FFMS_DestroyAudioSource(audio);
        FFMS_DestroyIndex(index);
        error = "Could not initialize FFMS2 audio conversion.";
        return false;
    }

    resample->ChannelLayout = FFMS_CH_FRONT_CENTER;
    resample->SampleFormat = FFMS_FMT_S16;
    if (FFMS_SetOutputFormatA(audio, resample, &errorInfo) != 0)
    {
        FFMS_DestroyResampleOptions(resample);
        FFMS_DestroyAudioSource(audio);
        FFMS_DestroyIndex(index);
        error = std::string("Could not convert audio to mono PCM: ") + errorInfo.Buffer;
        return false;
    }
    FFMS_DestroyResampleOptions(resample);

    auto const samples = original.NumSamples;
    auto const sampleRate = original.SampleRate;
    auto const actualBins = static_cast<size_t>((std::min<int64_t>)(samples, static_cast<int64_t>(bins)));
    auto const samplesPerBin = (samples + static_cast<int64_t>(actualBins) - 1) /
        static_cast<int64_t>(actualBins);

    std::ofstream stream(static_cast<std::filesystem::path const&>(output),
        std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        FFMS_DestroyAudioSource(audio);
        FFMS_DestroyIndex(index);
        error = "Could not create waveform output file.";
        return false;
    }

    double const duration = static_cast<double>(samples) / static_cast<double>(sampleRate);
    stream << "AEGISUB-WINUI-WAVEFORM\t1\t"
           << std::fixed << std::setprecision(6) << duration << '\t'
           << sampleRate << '\t' << actualBins << '\n';

    std::vector<int16_t> buffer(static_cast<size_t>(samplesPerBin));
    int64_t position = 0;

    for (size_t bin = 0; bin < actualBins; ++bin)
    {
        auto const count = (std::min<int64_t>)(samplesPerBin, samples - position);
        if (count <= 0)
            break;

        if (FFMS_GetAudio(audio, buffer.data(), position, count, &errorInfo) != 0)
        {
            FFMS_DestroyAudioSource(audio);
            FFMS_DestroyIndex(index);
            error = std::string("Audio decoding failed: ") + errorInfo.Buffer;
            return false;
        }

        int16_t minimum = 0;
        int16_t maximum = 0;
        for (int64_t sample = 0; sample < count; ++sample)
        {
            minimum = (std::min)(minimum, buffer[static_cast<size_t>(sample)]);
            maximum = (std::max)(maximum, buffer[static_cast<size_t>(sample)]);
        }

        stream << std::fixed << std::setprecision(6)
               << static_cast<double>(minimum) / 32768.0 << '\t'
               << static_cast<double>(maximum) / 32767.0 << '\n';
        position += count;
    }

    FFMS_DestroyAudioSource(audio);
    FFMS_DestroyIndex(index);
    return true;
#endif
}
