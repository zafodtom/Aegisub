#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace agi::winui {

struct SubtitleQualityFacts {
	bool empty{};
	bool invalid_interval{};
	bool too_short{};
	bool too_many_lines{};
	bool line_too_long{};
	bool too_fast{};
	bool edge_whitespace{};
	bool repeated_spaces{};
	bool unbalanced_braces{};
	bool overlaps_next{};
	size_t line_count{};
	size_t max_line_length{};
	size_t character_count{};
	double duration{};
	double cps{};
};

struct RecoveryDraftRow {
	size_t index{};
	std::string status;
	std::string target;
};

struct RecoveryDraft {
	uintmax_t file_size{};
	int64_t file_timestamp{};
	size_t row_count{};
	int32_t current_index{};
	bool workflow_dirty{};
	std::vector<RecoveryDraftRow> rows;
};

inline std::string EscapeBridgeField(std::string_view input);
inline std::string UnescapeBridgeField(std::string_view input);

inline std::string SerializeRecoveryDraft(RecoveryDraft const& draft) {
	std::ostringstream stream;
	stream << "AEGISUB-WINUI-DRAFT\t1\n";
	stream << "FILE\t" << draft.file_size << '\t' << draft.file_timestamp << '\n';
	stream << "ROWS\t" << draft.row_count << '\n';
	stream << "CURRENT\t" << draft.current_index << '\n';
	stream << "WORKFLOW\t" << (draft.workflow_dirty ? 1 : 0) << '\n';
	for (auto const& row : draft.rows) {
		stream << "ROW\t" << row.index << '\t'
			<< EscapeBridgeField(row.status) << '\t'
			<< EscapeBridgeField(row.target) << '\n';
	}
	return stream.str();
}

inline bool ParseRecoveryDraft(std::string_view input, RecoveryDraft& draft) {
	draft = {};
	std::istringstream stream{ std::string{ input } };
	std::string line;
	bool has_rows = false;
	bool has_current = false;
	bool has_workflow = false;
	if (!std::getline(stream, line) || line != "AEGISUB-WINUI-DRAFT\t1")
		return false;
	if (!std::getline(stream, line) || line.rfind("FILE\t", 0) != 0)
		return false;
	try {
		auto const separator = line.find('\t', 5);
		if (separator == std::string::npos)
			return false;
		draft.file_size = static_cast<uintmax_t>(std::stoull(line.substr(5, separator - 5)));
		draft.file_timestamp = std::stoll(line.substr(separator + 1));
		while (std::getline(stream, line)) {
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.rfind("ROWS\t", 0) == 0) {
				draft.row_count = static_cast<size_t>(std::stoull(line.substr(5)));
				has_rows = true;
			}
			else if (line.rfind("CURRENT\t", 0) == 0) {
				draft.current_index = std::stoi(line.substr(8));
				has_current = true;
			}
			else if (line.rfind("WORKFLOW\t", 0) == 0) {
				draft.workflow_dirty = line.substr(9) == "1";
				has_workflow = true;
			}
			else if (line.rfind("ROW\t", 0) == 0) {
				auto const index_separator = line.find('\t', 4);
				auto const status_separator = index_separator == std::string::npos
					? std::string::npos : line.find('\t', index_separator + 1);
				if (index_separator == std::string::npos || status_separator == std::string::npos)
					return false;
				RecoveryDraftRow row;
				row.index = static_cast<size_t>(std::stoull(line.substr(4, index_separator - 4)));
				row.status = UnescapeBridgeField(line.substr(
					index_separator + 1, status_separator - index_separator - 1));
				row.target = UnescapeBridgeField(line.substr(status_separator + 1));
				draft.rows.push_back(std::move(row));
			}
		}
	}
	catch (...) {
		return false;
	}
	return has_rows && has_current && has_workflow && draft.row_count == draft.rows.size();
}

inline double SubtitleOverlapQuality(double first_start, double first_end,
	double second_start, double second_end) {
	double const overlap = (std::min)(first_end, second_end) -
		(std::max)(first_start, second_start);
	double const shorter = (std::min)(first_end - first_start, second_end - second_start);
	return overlap > 0.0 && shorter > 0.0 ? overlap / shorter : 0.0;
}

inline bool ShouldPairSubtitles(double overlap_quality) {
	return overlap_quality >= 0.35;
}

inline bool ShouldKeepRecoveryArtifact(bool current, long long age_hours,
	size_t other_rank, size_t maximum_count = 30, long long maximum_age_hours = 24 * 30) {
	return current || (age_hours <= maximum_age_hours && other_rank < maximum_count);
}

inline SubtitleQualityFacts AnalyzeSubtitleQuality(std::wstring_view text,
	double start, double end, double next_start,
	size_t maximum_cpl = 42, double maximum_cps = 20.0) {
	SubtitleQualityFacts facts;
	facts.duration = end - start;
	facts.invalid_interval = facts.duration <= 0.0;
	facts.empty = text.empty() || std::all_of(text.begin(), text.end(), [](wchar_t character) {
		return std::iswspace(character) != 0;
	});
	facts.too_short = !facts.empty && facts.duration > 0.0 && facts.duration < 0.7;
	facts.edge_whitespace = !text.empty() &&
		(std::iswspace(text.front()) != 0 || std::iswspace(text.back()) != 0);

	facts.line_count = 1;
	size_t current_line_length = 0;
	int brace_depth = 0;
	for (size_t index = 0; index < text.size(); ++index) {
		wchar_t const character = text[index];
		if (character == L'\r')
			continue;
		if (character == L'\n') {
			++facts.line_count;
			facts.max_line_length = (std::max)(facts.max_line_length, current_line_length);
			current_line_length = 0;
			continue;
		}
		if (character == L'{' )
			++brace_depth;
		else if (character == L'}') {
			if (brace_depth == 0)
				facts.unbalanced_braces = true;
			else
				--brace_depth;
		}
		if (character == L' ' && index + 1 < text.size() && text[index + 1] == L' ')
			facts.repeated_spaces = true;
		++current_line_length;
		++facts.character_count;
	}
	facts.unbalanced_braces = facts.unbalanced_braces || brace_depth != 0;
	facts.max_line_length = (std::max)(facts.max_line_length, current_line_length);
	facts.too_many_lines = facts.line_count > 2;
	facts.line_too_long = facts.max_line_length > maximum_cpl;
	facts.cps = facts.duration > 0.0
		? static_cast<double>(facts.character_count) / facts.duration
		: 0.0;
	facts.too_fast = facts.cps > maximum_cps;
	facts.overlaps_next = next_start >= 0.0 && end > next_start + 0.0005;
	return facts;
}

inline std::string NormalizeSubtitleText(std::string_view input) {
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

inline std::string DenormalizeSubtitleText(std::string_view input) {
	std::string output;
	output.reserve(input.size());

	for (size_t i = 0; i < input.size(); ++i) {
		char const c = input[i];
		if (c == '\r') {
			if (i + 1 < input.size() && input[i + 1] == '\n')
				++i;
			output += "\\N";
		}
		else if (c == '\n') {
			output += "\\N";
		}
		else {
			output.push_back(c);
		}
	}

	return output;
}

inline std::string EscapeBridgeField(std::string_view input) {
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

inline std::string UnescapeBridgeField(std::string_view input) {
	std::string output;
	output.reserve(input.size());

	for (size_t i = 0; i < input.size(); ++i) {
		if (input[i] != '\\' || i + 1 >= input.size()) {
			output.push_back(input[i]);
			continue;
		}

		switch (input[++i]) {
		case '\\': output.push_back('\\'); break;
		case 't': output.push_back('\t'); break;
		case 'r': output.push_back('\r'); break;
		case 'n': output.push_back('\n'); break;
		default:
			output.push_back('\\');
			output.push_back(input[i]);
			break;
		}
	}

	return output;
}

inline bool EquivalentEditorText(std::wstring_view first, std::wstring_view second) {
	size_t firstIndex = 0;
	size_t secondIndex = 0;
	auto nextCharacter = [](std::wstring_view text, size_t& index) {
		wchar_t character = text[index++];
		if (character == L'\r') {
			if (index < text.size() && text[index] == L'\n')
				++index;
			return L'\n';
		}
		return character;
	};

	while (firstIndex < first.size() && secondIndex < second.size()) {
		if (nextCharacter(first, firstIndex) != nextCharacter(second, secondIndex))
			return false;
	}
	return firstIndex == first.size() && secondIndex == second.size();
}

inline std::wstring RebalanceSubtitleText(std::wstring_view input) {
	std::wstring flattened;
	flattened.reserve(input.size());

	for (size_t i = 0; i < input.size(); ++i) {
		if (input[i] != L'\r' && input[i] != L'\n') {
			flattened.push_back(input[i]);
			continue;
		}

		while (!flattened.empty() && std::iswspace(flattened.back()))
			flattened.pop_back();
		while (i + 1 < input.size() && std::iswspace(input[i + 1]))
			++i;
		if (!flattened.empty() && i + 1 < input.size())
			flattened.push_back(L' ');
	}

	size_t bestBegin = std::wstring::npos;
	size_t bestEnd = std::wstring::npos;
	size_t bestDifference = (std::numeric_limits<size_t>::max)();
	bool bestFirstLineLonger = false;

	for (size_t begin = 0; begin < flattened.size();) {
		if (!std::iswspace(flattened[begin])) {
			++begin;
			continue;
		}

		size_t end = begin + 1;
		while (end < flattened.size() && std::iswspace(flattened[end]))
			++end;

		if (begin > 0 && end < flattened.size()) {
			size_t const leftLength = begin;
			size_t const rightLength = flattened.size() - end;
			size_t const difference = leftLength > rightLength
			? leftLength - rightLength
			: rightLength - leftLength;
			bool const firstLineLonger = leftLength >= rightLength;

			if (difference < bestDifference
				|| (difference == bestDifference && firstLineLonger && !bestFirstLineLonger)) {
				bestBegin = begin;
				bestEnd = end;
				bestDifference = difference;
				bestFirstLineLonger = firstLineLonger;
			}
		}

		begin = end;
	}

	if (bestBegin == std::wstring::npos)
		return std::wstring{ input };

	flattened.replace(bestBegin, bestEnd - bestBegin, L"\r\n");
	return flattened;
}

}
