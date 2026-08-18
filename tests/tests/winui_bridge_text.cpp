#include <main.h>

#include "../../src/winui_bridge_text.h"

using namespace agi::winui;

TEST(winui_bridge_text, normalizes_ass_line_breaks) {
	EXPECT_EQ("first\nsecond\nthird", NormalizeSubtitleText("first\\Nsecond\\nthird"));
}

TEST(winui_bridge_text, normalizes_ass_hard_spaces_without_touching_other_escapes) {
	EXPECT_EQ("hard space\\q\\", NormalizeSubtitleText("hard\\hspace\\q\\"));
}

TEST(winui_bridge_text, denormalizes_all_editor_newline_styles) {
	EXPECT_EQ("one\\Ntwo\\Nthree\\Nfour", DenormalizeSubtitleText("one\r\ntwo\nthree\rfour"));
}

TEST(winui_bridge_text, bridge_field_round_trip_preserves_formatting_and_controls) {
	std::string const raw = "{\\i1}text{\\i0}\\Nnext\tcolumn\r\nline";
	EXPECT_EQ(raw, UnescapeBridgeField(EscapeBridgeField(raw)));
}

TEST(winui_bridge_text, unknown_and_trailing_escapes_are_preserved) {
	EXPECT_EQ("\\q\\", UnescapeBridgeField("\\q\\"));
}
