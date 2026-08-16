#include "subtitle_format.h"

#include "ass_dialogue.h"
#include "ass_file.h"

#include <libaegisub/format.h>
#include <libaegisub/fs.h>
#include <libaegisub/string.h>
#include <libaegisub/util.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <boost/algorithm/string/replace.hpp>

std::string float_to_string(double val, int precision) {
    std::string fmt = "%." + std::to_string(precision) + "f";
    std::string s = agi::format(fmt, val);
    size_t pos = s.find_last_not_of("0");
    if (pos != s.find(".")) ++pos;
    s.erase(begin(s) + pos, end(s));
    return s;
}

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

void SubtitleFormat::StripTags(AssFile &file) {
    for (auto& current : file.Events)
        current.StripTags();
}

void SubtitleFormat::ConvertNewlines(AssFile &file, std::string_view newline, bool mergeLineBreaks) {
    for (auto& current : file.Events) {
        std::string repl = current.Text;
        boost::replace_all(repl, "\\h", " ");
        boost::ireplace_all(repl, "\\n", newline);
        if (mergeLineBreaks) {
            auto dbl = agi::Str(newline, newline);
            size_t pos = 0;
            while ((pos = repl.find(dbl, pos)) != std::string::npos)
                boost::replace_all(repl, dbl, newline);
        }
        current.Text = repl;
    }
}

void SubtitleFormat::StripComments(AssFile &file) {
    file.Events.remove_and_dispose_if([](AssDialogue const& diag) {
        return diag.Comment || diag.Text.get().empty();
    }, [](AssDialogue *e) { delete e; });
}

void SubtitleFormat::RecombineOverlaps(AssFile &file) {
    auto cur = file.Events.begin();
    for (auto next = std::next(cur); next != file.Events.end(); cur = std::prev(next)) {
        if (next == file.Events.begin() || cur->End <= next->Start) {
            ++next;
            continue;
        }

        std::unique_ptr<AssDialogue> prevdlg(&*cur);
        std::unique_ptr<AssDialogue> curdlg(&*next);
        ++next;

        auto insert_line = [&](AssDialogue *newdlg) {
            file.Events.insert(std::find_if(next, file.Events.end(), [&](AssDialogue const& pos) {
                return pos.Start >= newdlg->Start;
            }), *newdlg);
        };

        if (curdlg->Start > prevdlg->Start) {
            auto newdlg = new AssDialogue(*prevdlg);
            newdlg->Start = prevdlg->Start;
            newdlg->End = curdlg->Start;
            newdlg->Text = prevdlg->Text;
            insert_line(newdlg);
        }

        {
            auto newdlg = new AssDialogue(*prevdlg);
            newdlg->Start = curdlg->Start;
            newdlg->End = (prevdlg->End < curdlg->End) ? prevdlg->End : curdlg->End;
            newdlg->Text = curdlg->Text.get() + "\\N" + prevdlg->Text.get();
            insert_line(newdlg);
        }

        if (prevdlg->End > curdlg->End) {
            auto newdlg = new AssDialogue(*prevdlg);
            newdlg->Start = curdlg->End;
            newdlg->End = prevdlg->End;
            newdlg->Text = prevdlg->Text;
            insert_line(newdlg);
        }

        if (curdlg->End > prevdlg->End) {
            auto newdlg = new AssDialogue(*prevdlg);
            newdlg->Start = prevdlg->End;
            newdlg->End = curdlg->End;
            newdlg->Text = curdlg->Text;
            insert_line(newdlg);
        }
    }
}

void SubtitleFormat::MergeIdentical(AssFile &file) {
    auto next = file.Events.begin();
    auto cur = next++;

    for (; next != file.Events.end(); cur = next++) {
        if (cur->End == next->Start && cur->Text == next->Text) {
            next->Start = (std::min)(next->Start, cur->Start);
            next->End = (std::max)(next->End, cur->End);
            delete &*cur;
        }
    }
}
