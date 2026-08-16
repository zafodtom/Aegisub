#include "ass_dialogue.h"
#include "ass_file.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "subtitle_format_ass.h"
#include "subtitle_format_srt.h"

#include <libaegisub/charset.h>
#include <libaegisub/exception.h>
#include <libaegisub/fs.h>
#include <libaegisub/log.h>
#include <libaegisub/option.h>
#include <libaegisub/path.h>
#include <libaegisub/vfr.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

namespace config {
    agi::Options *opt = nullptr;
    agi::MRUManager *mru = nullptr;
    agi::Path *path = nullptr;
    Automation4::AutoloadScriptManager *global_scripts = nullptr;
}

namespace {
std::string NormalizeSubtitleText(std::string_view input) {
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            const char next = input[i + 1];
            if (next == 'N' || next == 'n') {
                output.push_back('\n');
                ++i;
                continue;
            }
            if (next == 'h') {
                output.push_back(' ');
                ++i;
                continue;
            }
        }
        output.push_back(input[i]);
    }

    return output;
}

std::string EscapeField(std::string_view input) {
    std::string output;
    output.reserve(input.size());

    for (char c : input) {
        switch (c) {
        case '\\': output += "\\\\"; break;
        case '\t': output += "\\t"; break;
        case '\r': output += "\\r"; break;
        case '\n': output += "\\n"; break;
        default: output.push_back(c); break;
        }
    }

    return output;
}

std::string NormalizeTimestamp(agi::Time const& time) {
    std::string value = time.GetSrtFormatted();
    if (value.size() > 8)
        value[8] = '.';
    return value;
}

std::string DetectEncoding(agi::fs::path const& input) {
    auto encoding = agi::charset::Detect(input);
    if (encoding.empty())
        return "utf-8";
    return encoding;
}

void LoadSubtitles(agi::fs::path const& input, AssFile& file) {
    auto extension = input.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto encoding = DetectEncoding(input);
    agi::vfr::Framerate fps;

    if (extension == ".ass" || extension == ".ssa") {
        AssSubtitleFormat format;
        format.ReadFile(&file, input, fps, encoding.c_str());
        return;
    }

    if (extension == ".srt") {
        SRTSubtitleFormat format;
        format.ReadFile(&file, input, fps, encoding.c_str());
        return;
    }

    throw agi::InvalidInputException("WinUI bridge currently supports .ass, .ssa and .srt files only.");
}

void WriteBridgeFile(agi::fs::path const& output, AssFile const& file) {
    std::ofstream stream(static_cast<std::filesystem::path const&>(output), std::ios::binary | std::ios::trunc);
    if (!stream)
        throw agi::fs::FileNotAccessible(output);

    stream << "AEGISUB-WINUI-BRIDGE\t1\n";

    for (auto const& line : file.Events) {
        if (line.Comment)
            continue;

        auto text = NormalizeSubtitleText(line.GetStrippedText());
        stream << NormalizeTimestamp(line.Start) << '\t'
               << NormalizeTimestamp(line.End) << '\t'
               << EscapeField(text) << '\n';
    }
}
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 3)
        return 2;

    auto log = std::make_unique<agi::log::LogSink>();
    agi::log::log = log.get();

    auto path = std::make_unique<agi::Path>();
    config::path = path.get();

    auto options = std::make_unique<agi::Options>(
        agi::fs::path(),
        GET_DEFAULT_CONFIG(default_config),
        agi::Options::FLUSH_SKIP);
    config::opt = options.get();

    const agi::fs::path input{ std::filesystem::path(argv[1]) };
    const agi::fs::path output{ std::filesystem::path(argv[2]) };

    try {
        AssFile file;
        LoadSubtitles(input, file);
        WriteBridgeFile(output, file);
    }
    catch (agi::Exception const& e) {
        std::ofstream stream(static_cast<std::filesystem::path const&>(output), std::ios::binary | std::ios::trunc);
        if (stream)
            stream << "ERROR\t" << EscapeField(e.GetMessage()) << '\n';
        return 1;
    }
    catch (std::exception const& e) {
        std::ofstream stream(static_cast<std::filesystem::path const&>(output), std::ios::binary | std::ios::trunc);
        if (stream)
            stream << "ERROR\t" << EscapeField(e.what()) << '\n';
        return 1;
    }

    config::opt = nullptr;
    config::path = nullptr;
    agi::log::log = nullptr;
    return 0;
}
