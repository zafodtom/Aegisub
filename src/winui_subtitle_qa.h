#pragma once

#include "winui_bridge_text.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

namespace agi::winui {

struct SubtitleQaSettings {
	size_t maximum_cpl{42};
	double maximum_cps{20.0};
	double minimum_duration{0.7};
	size_t maximum_lines{2};
	double minimum_length_ratio{0.35};
	double maximum_length_ratio{2.5};
	bool check_terminal_punctuation{true};
	bool check_number_tokens{true};
	bool check_spacing{true};
	bool check_repeated_punctuation{true};
	bool check_length_ratio{true};
	bool check_czech_quotes{false};
};

enum class SubtitleQaIssue {
	empty,
	invalid_interval,
	too_short,
	too_many_lines,
	line_too_long,
	too_fast,
	edge_whitespace,
	repeated_spaces,
	unbalanced_braces,
	overlaps_next,
	space_before_punctuation,
	repeated_punctuation,
	terminal_punctuation_mismatch,
	number_token_mismatch,
	length_ratio,
	straight_quotes,
};

struct SubtitleQaReport {
	SubtitleQualityFacts facts;
	std::vector<SubtitleQaIssue> issues;
	double length_ratio{1.0};

	bool ok() const { return issues.empty(); }
	bool has(SubtitleQaIssue issue) const {
		return std::find(issues.begin(), issues.end(), issue) != issues.end();
	}
};

inline bool IsSubtitlePunctuation(wchar_t c) {
	return c == L'.' || c == L',' || c == L'?' || c == L'!' || c == L':' || c == L';';
}

inline size_t VisibleCharacterCount(std::wstring_view text) {
	return static_cast<size_t>(std::count_if(text.begin(), text.end(), [](wchar_t c) {
		return !std::iswspace(c);
	}));
}

inline bool HasSpaceBeforePunctuation(std::wstring_view text) {
	for (size_t index = 1; index < text.size(); ++index) {
		if (IsSubtitlePunctuation(text[index]) && std::iswspace(text[index - 1]))
			return true;
	}
	return false;
}

inline bool HasRepeatedPunctuation(std::wstring_view text) {
	for (size_t index = 1; index < text.size(); ++index) {
		if (IsSubtitlePunctuation(text[index]) && text[index] == text[index - 1])
			return true;
	}
	return false;
}

inline std::vector<std::wstring> ProtectedTokens(std::wstring_view text) {
	std::vector<std::wstring> tokens;
	for (size_t position = 0; position < text.size();) {
		if (!std::iswdigit(text[position])) {
			++position;
			continue;
		}
		auto const begin = position++;
		while (position < text.size()) {
			auto const c = text[position];
			if (std::iswdigit(c) || c == L':' || c == L'.' || c == L',' || c == L'%' ||
				c == L'€' || c == L'$' || c == L'£' || c == L'K' || c == L'č') {
				++position;
				continue;
			}
			break;
		}
		tokens.emplace_back(text.substr(begin, position - begin));
	}
	std::sort(tokens.begin(), tokens.end());
	return tokens;
}

inline SubtitleQaReport AnalyzeTranslationQuality(
	std::wstring_view source,
	std::wstring_view target,
	double start,
	double end,
	double next_start,
	SubtitleQaSettings const& settings = {}) {
	SubtitleQaReport report;
	report.facts = AnalyzeSubtitleQuality(target, start, end, next_start,
		settings.maximum_cpl, settings.maximum_cps);

	auto add = [&](SubtitleQaIssue issue, bool condition) {
		if (condition)
			report.issues.push_back(issue);
	};

	add(SubtitleQaIssue::empty, report.facts.empty);
	add(SubtitleQaIssue::invalid_interval, report.facts.invalid_interval);
	add(SubtitleQaIssue::too_short,
		!report.facts.empty && report.facts.duration > 0.0 && report.facts.duration < settings.minimum_duration);
	add(SubtitleQaIssue::too_many_lines, report.facts.line_count > settings.maximum_lines);
	add(SubtitleQaIssue::line_too_long, report.facts.max_line_length > settings.maximum_cpl);
	add(SubtitleQaIssue::too_fast, report.facts.cps > settings.maximum_cps);
	add(SubtitleQaIssue::edge_whitespace, report.facts.edge_whitespace);
	add(SubtitleQaIssue::repeated_spaces, settings.check_spacing && report.facts.repeated_spaces);
	add(SubtitleQaIssue::unbalanced_braces, report.facts.unbalanced_braces);
	add(SubtitleQaIssue::overlaps_next, report.facts.overlaps_next);
	add(SubtitleQaIssue::space_before_punctuation,
		settings.check_spacing && HasSpaceBeforePunctuation(target));
	add(SubtitleQaIssue::repeated_punctuation,
		settings.check_repeated_punctuation && HasRepeatedPunctuation(target));

	if (settings.check_terminal_punctuation && !source.empty() && !target.empty()) {
		auto const source_punctuation = TerminalPunctuation(source);
		auto const target_punctuation = TerminalPunctuation(target);
		add(SubtitleQaIssue::terminal_punctuation_mismatch,
			source_punctuation != wchar_t{} && source_punctuation != target_punctuation);
	}

	if (settings.check_number_tokens)
		add(SubtitleQaIssue::number_token_mismatch, ProtectedTokens(source) != ProtectedTokens(target));

	auto const source_count = VisibleCharacterCount(source);
	auto const target_count = VisibleCharacterCount(target);
	if (source_count > 0) {
		report.length_ratio = static_cast<double>(target_count) / static_cast<double>(source_count);
		add(SubtitleQaIssue::length_ratio, settings.check_length_ratio && !report.facts.empty &&
			(report.length_ratio < settings.minimum_length_ratio || report.length_ratio > settings.maximum_length_ratio));
	}

	if (settings.check_czech_quotes)
		add(SubtitleQaIssue::straight_quotes, target.find(L'"') != std::wstring_view::npos);

	return report;
}

inline std::wstring SubtitleQaIssueLabel(SubtitleQaIssue issue) {
	switch (issue) {
		case SubtitleQaIssue::empty: return L"prázdný překlad";
		case SubtitleQaIssue::invalid_interval: return L"neplatný časový interval";
		case SubtitleQaIssue::too_short: return L"příliš krátké zobrazení";
		case SubtitleQaIssue::too_many_lines: return L"příliš mnoho řádků";
		case SubtitleQaIssue::line_too_long: return L"překročen CPL";
		case SubtitleQaIssue::too_fast: return L"překročen CPS";
		case SubtitleQaIssue::edge_whitespace: return L"mezera na začátku nebo konci";
		case SubtitleQaIssue::repeated_spaces: return L"opakované mezery";
		case SubtitleQaIssue::unbalanced_braces: return L"nevyvážené formátovací závorky";
		case SubtitleQaIssue::overlaps_next: return L"časový překryv";
		case SubtitleQaIssue::space_before_punctuation: return L"mezera před interpunkcí";
		case SubtitleQaIssue::repeated_punctuation: return L"zdvojená interpunkce";
		case SubtitleQaIssue::terminal_punctuation_mismatch: return L"odlišná koncová interpunkce";
		case SubtitleQaIssue::number_token_mismatch: return L"změněné číslo, čas nebo jednotka";
		case SubtitleQaIssue::length_ratio: return L"neobvyklý poměr délky překladu";
		case SubtitleQaIssue::straight_quotes: return L"rovné uvozovky v českém textu";
	}
	return L"QA upozornění";
}

inline std::wstring SubtitleQaSummary(SubtitleQaReport const& report, size_t maximum_items = 3) {
	std::wstring summary;
	for (size_t index = 0; index < report.issues.size() && index < maximum_items; ++index) {
		if (!summary.empty())
			summary += L" · ";
		summary += SubtitleQaIssueLabel(report.issues[index]);
	}
	if (report.issues.size() > maximum_items)
		summary += L" · +" + std::to_wstring(report.issues.size() - maximum_items);
	return summary;
}

}
