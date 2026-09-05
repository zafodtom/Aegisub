#include <main.h>

#include "../../src/winui_bridge_text.h"
#include "../../src/winui_locale.h"

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

TEST(winui_bridge_text, editor_text_comparison_ignores_newline_representation) {
	EXPECT_TRUE(EquivalentEditorText(L"prvn\u00ED\ndruh\u00FD", L"prvn\u00ED\r\ndruh\u00FD"));
	EXPECT_TRUE(EquivalentEditorText(L"prvn\u00ED\rdruh\u00FD", L"prvn\u00ED\ndruh\u00FD"));
	EXPECT_FALSE(EquivalentEditorText(L"prvn\u00ED\ndruh\u00FD", L"prvn\u00ED\nt\u0159et\u00ED"));
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

TEST(winui_workflow, overlap_quality_uses_the_shorter_subtitle) {
	EXPECT_DOUBLE_EQ(1.0, SubtitleOverlapQuality(10.0, 14.0, 11.0, 13.0));
	EXPECT_DOUBLE_EQ(0.0, SubtitleOverlapQuality(10.0, 11.0, 12.0, 13.0));
	EXPECT_TRUE(ShouldPairSubtitles(0.35));
	EXPECT_FALSE(ShouldPairSubtitles(0.34));
}

TEST(winui_workflow, quality_detects_timing_text_and_formatting_problems) {
	auto const facts = AnalyzeSubtitleQuality(L"  {\\i1P\u0159\u00EDli\u0161  rychl\u00FD text", 2.0, 2.5, 2.4, 12, 10.0);
	EXPECT_TRUE(facts.too_short);
	EXPECT_TRUE(facts.line_too_long);
	EXPECT_TRUE(facts.too_fast);
	EXPECT_TRUE(facts.edge_whitespace);
	EXPECT_TRUE(facts.repeated_spaces);
	EXPECT_TRUE(facts.unbalanced_braces);
	EXPECT_TRUE(facts.overlaps_next);
}

TEST(winui_workflow, quality_accepts_a_clean_subtitle) {
	auto const facts = AnalyzeSubtitleQuality(L"Kr\u00E1tk\u00FD text", 2.0, 4.0, 4.2);
	EXPECT_FALSE(facts.empty);
	EXPECT_FALSE(facts.invalid_interval);
	EXPECT_FALSE(facts.too_short);
	EXPECT_FALSE(facts.too_fast);
	EXPECT_FALSE(facts.unbalanced_braces);
	EXPECT_FALSE(facts.overlaps_next);
}

TEST(winui_workflow, subtitle_filters_select_the_expected_rows) {
	EXPECT_TRUE(MatchesSubtitleFilter(SubtitleFilter::all, false, false, false, false, false));
	EXPECT_TRUE(MatchesSubtitleFilter(SubtitleFilter::untranslated, true, false, true, false, false));
	EXPECT_TRUE(MatchesSubtitleFilter(SubtitleFilter::modified, false, true, false, false, false));
	EXPECT_TRUE(MatchesSubtitleFilter(SubtitleFilter::problems, false, false, true, true, false));
	EXPECT_TRUE(MatchesSubtitleFilter(SubtitleFilter::ready, false, false, false, true, false));
	EXPECT_FALSE(MatchesSubtitleFilter(SubtitleFilter::ready, false, false, true, true, false));
	EXPECT_TRUE(MatchesSubtitleFilter(SubtitleFilter::approved, false, false, false, false, true));
	EXPECT_FALSE(MatchesSubtitleFilter(SubtitleFilter::approved, false, false, true, false, true));
}

TEST(winui_workflow, initializes_unicode_ctype_for_czech_workflow_text) {
	ASSERT_TRUE(InitializeTextLocale());
	EXPECT_EQ(2U, CountCaseInsensitiveMatches(L"\u010CAS \u010Das", L"\u010Das"));
	EXPECT_EQ(
		L"p\u0159\u00EDli\u0161 \u017Elu\u0165ou\u010Dk\u00FD k\u016F\u0148",
		ConsistencyTextKey(L"P\u0158\u00CDLI\u0160 \u017DLU\u0164OU\u010CK\u00DD K\u016E\u0147"));
}

TEST(winui_workflow, bulk_replace_is_case_insensitive_and_non_overlapping) {
	EXPECT_EQ(3U, CountCaseInsensitiveMatches(L"Test test TEST", L"test"));
	EXPECT_EQ(L"X X X", ReplaceCaseInsensitive(L"Test test TEST", L"test", L"X"));
	EXPECT_EQ(L"bbb", ReplaceCaseInsensitive(L"aaaaaa", L"aa", L"b"));
	EXPECT_EQ(L"beze zm\u011Bny", ReplaceCaseInsensitive(L"beze zm\u011Bny", L"", L"X"));
}

TEST(winui_workflow, consistency_helpers_normalize_text_and_find_tokens) {
	EXPECT_EQ(L"same text", ConsistencyTextKey(L"  SAME\t text  "));
	auto const numbers = NumberTokens(L"Let 12 trv\u00E1 03:45");
	ASSERT_EQ(3U, numbers.size());
	EXPECT_EQ(L"12", numbers[0]);
	EXPECT_EQ(L'?', TerminalPunctuation(L"Opravdu?  "));
	EXPECT_EQ(wchar_t{}, TerminalPunctuation(L"Bez te\u010Dky"));
}

TEST(winui_workflow, recovery_retention_protects_current_and_limits_old_files) {
	EXPECT_TRUE(ShouldKeepRecoveryArtifact(true, 24 * 365, 99));
	EXPECT_TRUE(ShouldKeepRecoveryArtifact(false, 24, 0));
	EXPECT_FALSE(ShouldKeepRecoveryArtifact(false, 24 * 31, 0));
	EXPECT_FALSE(ShouldKeepRecoveryArtifact(false, 24, 30));
}

TEST(winui_workflow, recovery_draft_round_trip_preserves_all_rows) {
	RecoveryDraft input;
	input.file_size = 12345;
	input.file_timestamp = -987654321;
	input.row_count = 2;
	input.current_index = 1;
	input.workflow_dirty = true;
	input.rows = {
		{ 0, "P\xC5\x99ipraveno", "Prvn\xC3\xAD\n\xC5\x99\xC3\xA1" "dek" },
		{ 1, "Schv\xC3\xA1leno", "Text\tse\\znaky" },
	};
	RecoveryDraft output;
	ASSERT_TRUE(ParseRecoveryDraft(SerializeRecoveryDraft(input), output));
	EXPECT_EQ(input.file_size, output.file_size);
	EXPECT_EQ(input.file_timestamp, output.file_timestamp);
	EXPECT_EQ(input.current_index, output.current_index);
	EXPECT_TRUE(output.workflow_dirty);
	ASSERT_EQ(2U, output.rows.size());
	EXPECT_EQ(input.rows[0].target, output.rows[0].target);
	EXPECT_EQ(input.rows[1].status, output.rows[1].status);
}

TEST(winui_workflow, recovery_draft_rejects_truncated_data) {
	RecoveryDraft draft;
	EXPECT_FALSE(ParseRecoveryDraft("AEGISUB-WINUI-DRAFT\t1\nFILE\t12", draft));
}

TEST(winui_workflow, recovery_draft_rejects_missing_sections) {
	RecoveryDraft draft;
	EXPECT_FALSE(ParseRecoveryDraft(
		"AEGISUB-WINUI-DRAFT\t1\nFILE\t12\t34\nROWS\t0\n", draft));
}
