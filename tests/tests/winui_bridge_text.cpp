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

TEST(winui_bridge_text, balances_a_subtitle_at_the_nearest_word_boundary) {
	EXPECT_EQ(L"Jedna dva\r\nt\u0159i \u010Dty\u0159i", RebalanceSubtitleText(L"Jedna dva t\u0159i \u010Dty\u0159i"));
}

TEST(winui_bridge_text, recalculates_an_existing_line_break) {
	EXPECT_EQ(L"Jedna dva\r\nt\u0159i \u010Dty\u0159i", RebalanceSubtitleText(L"Jedna\r\ndva t\u0159i \u010Dty\u0159i"));
}

TEST(winui_bridge_text, prefers_a_longer_first_line_when_splits_are_equally_balanced) {
	EXPECT_EQ(L"123 456\r\n789", RebalanceSubtitleText(L"123 456 789"));
}

TEST(winui_bridge_text, leaves_a_single_word_unchanged) {
	EXPECT_EQ(L"Nep\u0159eru\u0161iteln\u00E9", RebalanceSubtitleText(L"Nep\u0159eru\u0161iteln\u00E9"));
}
