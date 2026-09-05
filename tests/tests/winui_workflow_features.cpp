#include <main.h>

#include "../../src/winui_locale.h"
#include "../../src/winui_project_state.h"
#include "../../src/winui_recovery.h"
#include "../../src/winui_search_replace.h"
#include "../../src/winui_subtitle_io.h"
#include "../../src/winui_subtitle_qa.h"
#include "../../src/winui_subtitle_workflow.h"

using namespace agi::winui;

TEST(winui_workflow_features, qa_settings_control_cpl_cps_duration_and_lines) {
	SubtitleQaSettings settings;
	settings.maximum_cpl = 7;
	settings.maximum_cps = 5.0;
	settings.minimum_duration = 1.0;
	settings.maximum_lines = 1;
	auto const report = AnalyzeTranslationQuality(
		L"Source 10?", L"Text 10?\nDruhý", 0.0, 0.8, 1.0, settings);
	EXPECT_TRUE(report.has(SubtitleQaIssue::too_short));
	EXPECT_TRUE(report.has(SubtitleQaIssue::too_many_lines));
	EXPECT_TRUE(report.has(SubtitleQaIssue::line_too_long));
	EXPECT_TRUE(report.has(SubtitleQaIssue::too_fast));
}

TEST(winui_workflow_features, qa_detects_translation_consistency_hazards) {
	SubtitleQaSettings settings;
	settings.check_czech_quotes = true;
	auto const report = AnalyzeTranslationQuality(
		L"Pay 12.50 € now!", L"\"Zaplať 15.00 € teď !!\"", 0.0, 3.0, -1.0, settings);
	EXPECT_TRUE(report.has(SubtitleQaIssue::number_token_mismatch));
	EXPECT_TRUE(report.has(SubtitleQaIssue::repeated_punctuation));
	EXPECT_TRUE(report.has(SubtitleQaIssue::straight_quotes));
}

TEST(winui_workflow_features, qa_detects_spacing_and_terminal_punctuation) {
	auto const report = AnalyzeTranslationQuality(
		L"Really?", L"Opravdu !", 0.0, 2.0, -1.0);
	EXPECT_TRUE(report.has(SubtitleQaIssue::space_before_punctuation));
	EXPECT_TRUE(report.has(SubtitleQaIssue::terminal_punctuation_mismatch));
}

TEST(winui_workflow_features, search_supports_scope_case_and_whole_words) {
	ASSERT_TRUE(InitializeTextLocale());
	SearchOptions options;
	options.scope = SearchScope::both;
	EXPECT_TRUE(SearchRowMatches(L"ČAS běží", L"Máme čas", L"čas", options));
	options.case_sensitive = true;
	EXPECT_FALSE(SearchRowMatches(L"ČAS běží", L"", L"čas", options));
	options.case_sensitive = false;
	options.whole_word = true;
	EXPECT_EQ(1U, CountSearchMatches(L"čas časování", L"čas", options));
}

TEST(winui_workflow_features, replace_respects_whole_words) {
	SearchOptions options;
	options.whole_word = true;
	EXPECT_EQ(L"doba časování doba", ReplaceSearchMatches(
		L"čas časování ČAS", L"čas", L"doba", options));
}

TEST(winui_workflow_features, workflow_summary_counts_actionable_states) {
	std::vector<WorkflowRowState> rows{
		{true, false, false, true, false},
		{true, true, true, false, false},
		{true, false, true, false, true},
		{false, false, false, false, true},
	};
	auto const summary = SummarizeWorkflow(rows);
	EXPECT_EQ(4U, summary.total);
	EXPECT_EQ(3U, summary.translated);
	EXPECT_EQ(1U, summary.modified);
	EXPECT_EQ(1U, summary.ready);
	EXPECT_EQ(1U, summary.approved);
	EXPECT_EQ(2U, summary.problems);
	EXPECT_DOUBLE_EQ(75.0, summary.completion_percent);
}

TEST(winui_workflow_features, workflow_navigation_wraps_in_both_directions) {
	std::vector<WorkflowRowState> rows(4);
	rows[1].has_problem = true;
	EXPECT_EQ(1, FindWorkflowRow(rows, 2, 1, [](auto const& row) { return row.has_problem; }));
	EXPECT_EQ(1, FindWorkflowRow(rows, 0, -1, [](auto const& row) { return row.has_problem; }));
}

TEST(winui_workflow_features, pairing_quality_flags_weak_and_ambiguous_matches) {
	EXPECT_EQ(SubtitlePairQuality::missing, AssessSubtitlePair(0.0, 0));
	EXPECT_EQ(SubtitlePairQuality::weak, AssessSubtitlePair(0.4, 1));
	EXPECT_EQ(SubtitlePairQuality::good, AssessSubtitlePair(0.6, 1));
	EXPECT_EQ(SubtitlePairQuality::excellent, AssessSubtitlePair(0.9, 1));
	EXPECT_EQ(SubtitlePairQuality::ambiguous, AssessSubtitlePair(0.6, 2));
}

TEST(winui_workflow_features, recent_projects_upgrade_v1_and_keep_mru_order) {
	std::vector<RecentTranslationProject> projects;
	ASSERT_TRUE(ParseRecentProjects(
		"AEGISUB-WINUI-RECENT\t1\nSOURCE\tsource.srt\nTARGET\ttarget.srt\n", projects));
	ASSERT_EQ(1U, projects.size());
	TouchRecentProject(projects, {"new.en.srt", "new.cs.srt"}, 3);
	TouchRecentProject(projects, {"source.srt", "target.srt"}, 3);
	ASSERT_EQ(2U, projects.size());
	EXPECT_EQ("source.srt", projects.front().source_path);
	std::vector<RecentTranslationProject> parsed;
	ASSERT_TRUE(ParseRecentProjects(SerializeRecentProjects(projects), parsed));
	EXPECT_EQ(projects.size(), parsed.size());
}

TEST(winui_workflow_features, settings_round_trip_preserves_qa_and_editor_preferences) {
	WinUiWorkspaceSettings input;
	input.qa.maximum_cpl = 38;
	input.qa.maximum_cps = 17.5;
	input.qa.minimum_duration = 0.9;
	input.qa.maximum_lines = 3;
	input.qa.check_czech_quotes = true;
	input.autosave_draft = false;
	input.editor_font_size = 27.0;
	input.default_directory = "C:\\Subtitles\\Project";
	WinUiWorkspaceSettings output;
	ASSERT_TRUE(ParseWorkspaceSettings(SerializeWorkspaceSettings(input), output));
	EXPECT_EQ(38U, output.qa.maximum_cpl);
	EXPECT_DOUBLE_EQ(17.5, output.qa.maximum_cps);
	EXPECT_EQ(3U, output.qa.maximum_lines);
	EXPECT_TRUE(output.qa.check_czech_quotes);
	EXPECT_FALSE(output.autosave_draft);
	EXPECT_DOUBLE_EQ(27.0, output.editor_font_size);
	EXPECT_EQ(input.default_directory, output.default_directory);
}

TEST(winui_workflow_features, io_backend_capabilities_allow_bridge_to_be_replaced) {
	auto const bridge = BridgeSubtitleIoCapabilities();
	auto const direct = DirectLibrarySubtitleIoCapabilities();
	EXPECT_FALSE(bridge.runs_in_process);
	EXPECT_TRUE(direct.runs_in_process);
	EXPECT_TRUE(PreferDirectSubtitleIo(true));
	EXPECT_FALSE(PreferDirectSubtitleIo(false));
}

TEST(winui_workflow_features, recovery_artifacts_prioritize_current_project) {
	std::vector<RecoveryArtifactInfo> artifacts{
		{RecoveryArtifactKind::backup, L"old", 20, 10, false},
		{RecoveryArtifactKind::draft, L"current", 10, 10, true},
	};
	SortRecoveryArtifacts(artifacts);
	EXPECT_TRUE(artifacts.front().current_project);
}
