#include "ass_dialogue.h"
#include "ass_file.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "subtitle_format_ass.h"
#include "subtitle_format_srt.h"
#include "subtitle_format_ssa.h"
#include "winui_bridge_text.h"

#include <libaegisub/charset.h>
#include <libaegisub/dispatch.h>
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
#include <vector>

namespace config {
    agi::Options *opt = nullptr;
    agi::MRUManager *mru = nullptr;
    agi::Path *path = nullptr;
    Automation4::AutoloadScriptManager *global_scripts = nullptr;
}

namespace {
using agi::winui::DenormalizeSubtitleText;
using agi::winui::EscapeBridgeField;
using agi::winui::NormalizeSubtitleText;
using agi::winui::UnescapeBridgeField;

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

std::string LowerExtension(agi::fs::path const& input) {
    auto extension = input.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

void LoadSubtitles(agi::fs::path const& input, AssFile& file) {
    auto const extension = LowerExtension(input);
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

void SaveSubtitles(agi::fs::path const& templateFile, agi::fs::path const& output, AssFile const& file) {
    auto const extension = LowerExtension(templateFile);
    auto encoding = DetectEncoding(templateFile);
    agi::vfr::Framerate fps;

    if (extension == ".ass") {
        AssSubtitleFormat format;
        format.WriteFile(&file, output, fps, encoding.c_str());
        return;
    }

    if (extension == ".ssa") {
        SsaSubtitleFormat format;
        format.WriteFile(&file, output, fps, encoding.c_str());
        return;
    }

    if (extension == ".srt") {
        SRTSubtitleFormat format;
        format.WriteFile(&file, output, fps, encoding.c_str());
        return;
    }

    throw agi::InvalidInputException("WinUI bridge currently supports writing .ass, .ssa and .srt files only.");
}

void WriteBridgeFile(agi::fs::path const& output, AssFile const& file) {
    std::ofstream stream(static_cast<std::filesystem::path const&>(output), std::ios::binary | std::ios::trunc);
    if (!stream)
        throw agi::fs::FileNotAccessible(output);

    // v2 adds the raw Aegisub dialogue text as a fourth field.  The WinUI
    // frontend displays the stripped third field, but can round-trip unchanged
    // ASS/SSA/SRT formatting using the raw field.
    stream << "AEGISUB-WINUI-BRIDGE\t2\n";

    for (auto const& line : file.Events) {
        if (line.Comment)
            continue;

        auto displayText = NormalizeSubtitleText(line.GetStrippedText());
        stream << NormalizeTimestamp(line.Start) << '\t'
               << NormalizeTimestamp(line.End) << '\t'
               << EscapeBridgeField(displayText) << '\t'
               << EscapeBridgeField(line.Text.get()) << '\n';
    }
}

void ApplyBridgeFile(agi::fs::path const& input, AssFile& file) {
    std::ifstream stream(static_cast<std::filesystem::path const&>(input), std::ios::binary);
    if (!stream)
        throw agi::fs::FileNotAccessible(input);

    std::string line;
    if (!std::getline(stream, line))
        throw agi::InvalidInputException("WinUI update file is empty.");
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    if (line != "AEGISUB-WINUI-BRIDGE\t1" && line != "AEGISUB-WINUI-BRIDGE\t2")
        throw agi::InvalidInputException("WinUI update file has an unknown format.");

    std::vector<AssDialogue*> dialogues;
    for (auto& dialogue : file.Events) {
        if (!dialogue.Comment)
            dialogues.push_back(&dialogue);
    }

    size_t index = 0;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        auto const firstTab = line.find('\t');
        auto const secondTab = firstTab == std::string::npos
            ? std::string::npos
            : line.find('\t', firstTab + 1);
        if (firstTab == std::string::npos || secondTab == std::string::npos)
            throw agi::InvalidInputException("WinUI update file contains an invalid row.");
        if (index >= dialogues.size())
            throw agi::InvalidInputException("WinUI update file contains more dialogue rows than the subtitle template.");

        auto const start = line.substr(0, firstTab);
        auto const end = line.substr(firstTab + 1, secondTab - firstTab - 1);
        auto const text = UnescapeBridgeField(std::string_view(line).substr(secondTab + 1));

        auto& dialogue = *dialogues[index++];
        dialogue.Start = std::string_view(start);
        dialogue.End = std::string_view(end);
        dialogue.Text = DenormalizeSubtitleText(text);
    }

    if (index != dialogues.size())
        throw agi::InvalidInputException("WinUI update file contains fewer dialogue rows than the subtitle template.");
}

void WriteErrorFile(agi::fs::path const& output, std::string_view message) {
    std::ofstream stream(static_cast<std::filesystem::path const&>(output), std::ios::binary | std::ios::trunc);
    if (stream)
        stream << "ERROR\t" << EscapeBridgeField(message) << '\n';
}
}

int wmain(int argc, wchar_t **argv) {
    bool const writeMode = argc == 5 && std::wstring_view(argv[1]) == L"--write";
    bool const readMode = argc == 3;
    if (!writeMode && !readMode)
        return 2;

    const agi::fs::path input{ std::filesystem::path(argv[writeMode ? 2 : 1]) };
    const agi::fs::path update{ writeMode ? std::filesystem::path(argv[3]) : std::filesystem::path() };
    const agi::fs::path output{ std::filesystem::path(argv[writeMode ? 4 : 2]) };

    try {
        // Aegisub's LogSink creates a serial dispatch queue in its constructor,
        // so dispatch must be initialized first just as it is in AegisubApp::OnInit.
        // The bridge has no GUI main loop, so main-thread callbacks can execute
        // immediately.
        agi::dispatch::Init([](agi::dispatch::Thunk thunk) {
            thunk();
        });

        auto path = std::make_unique<agi::Path>();
        config::path = path.get();

        auto log = std::make_unique<agi::log::LogSink>();
        agi::log::log = log.get();

        auto options = std::make_unique<agi::Options>(
            agi::fs::path(),
            GET_DEFAULT_CONFIG(default_config),
            agi::Options::FLUSH_SKIP);
        config::opt = options.get();

        AssFile file;
        LoadSubtitles(input, file);

        if (writeMode) {
            ApplyBridgeFile(update, file);
            SaveSubtitles(input, output, file);
        }
        else {
            WriteBridgeFile(output, file);
        }

        config::opt = nullptr;
        agi::log::log = nullptr;
        config::path = nullptr;
    }
    catch (agi::Exception const& e) {
        config::opt = nullptr;
        agi::log::log = nullptr;
        config::path = nullptr;
        WriteErrorFile(output, e.GetMessage());
        return 1;
    }
    catch (std::exception const& e) {
        config::opt = nullptr;
        agi::log::log = nullptr;
        config::path = nullptr;
        WriteErrorFile(output, e.what());
        return 1;
    }

    return 0;
}
