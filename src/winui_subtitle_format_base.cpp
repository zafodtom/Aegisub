#include "subtitle_format.h"

#include "ass_dialogue.h"
#include "ass_file.h"

#include <libaegisub/fs.h>
#include <libaegisub/util.h>

#include <boost/algorithm/string/replace.hpp>

SubtitleFormat::SubtitleFormat(std::string_view name)
: name(name)
{
}

bool SubtitleFormat::CanReadFile(agi::fs::path const& filename, const char *) const {
    auto wildcards = GetReadWildcards();
    return agi::util::any_of(wildcards,
        [&](std::string const& ext) { return agi::fs::HasExtension(filename, ext); });
}

bool SubtitleFormat::CanWriteFile(agi::fs::path const& filename) const {
    auto wildcards = GetWriteWildcards();
    return agi::util::any_of(wildcards,
        [&](std::string const& ext) { return agi::fs::HasExtension(filename, ext); });
}

bool SubtitleFormat::CanSave(const AssFile *subs) const {
    if (!subs->Attachments.empty())
        return false;

    auto def = boost::flyweight<std::string>("Default");
    for (auto const& line : subs->Events) {
        if (line.Style != def || line.GetStrippedText() != line.Text)
            return false;
    }

    return true;
}
