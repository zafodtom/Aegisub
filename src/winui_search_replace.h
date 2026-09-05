#pragma once

#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

namespace agi::winui {

enum class SearchScope {
	target,
	source,
	both,
};

struct SearchOptions {
	SearchScope scope{SearchScope::both};
	bool case_sensitive{false};
	bool whole_word{false};
};

struct SearchMatch {
	size_t position{};
	size_t length{};
};

inline bool SearchWordCharacter(wchar_t c) {
	return std::iswalnum(c) != 0 || c == L'_';
}

inline bool SearchCharactersEqual(wchar_t left, wchar_t right, bool case_sensitive) {
	return case_sensitive ? left == right : std::towlower(left) == std::towlower(right);
}

inline bool SearchMatchAt(std::wstring_view text, std::wstring_view query, size_t position,
	SearchOptions const& options = {}) {
	if (query.empty() || position + query.size() > text.size())
		return false;
	for (size_t index = 0; index < query.size(); ++index) {
		if (!SearchCharactersEqual(text[position + index], query[index], options.case_sensitive))
			return false;
	}
	if (!options.whole_word)
		return true;
	bool const left_ok = position == 0 || !SearchWordCharacter(text[position - 1]);
	auto const right = position + query.size();
	bool const right_ok = right == text.size() || !SearchWordCharacter(text[right]);
	return left_ok && right_ok;
}

inline std::vector<SearchMatch> FindSearchMatches(std::wstring_view text, std::wstring_view query,
	SearchOptions const& options = {}) {
	std::vector<SearchMatch> matches;
	if (query.empty())
		return matches;
	for (size_t position = 0; position + query.size() <= text.size();) {
		if (SearchMatchAt(text, query, position, options)) {
			matches.push_back({position, query.size()});
			position += query.size();
		}
		else {
			++position;
		}
	}
	return matches;
}

inline size_t CountSearchMatches(std::wstring_view text, std::wstring_view query,
	SearchOptions const& options = {}) {
	return FindSearchMatches(text, query, options).size();
}

inline bool SearchRowMatches(std::wstring_view source, std::wstring_view target,
	std::wstring_view query, SearchOptions const& options = {}) {
	auto has = [&](std::wstring_view text) {
		return !FindSearchMatches(text, query, options).empty();
	};
	switch (options.scope) {
		case SearchScope::source: return has(source);
		case SearchScope::target: return has(target);
		case SearchScope::both: return has(source) || has(target);
	}
	return false;
}

inline std::wstring ReplaceSearchMatches(std::wstring_view text, std::wstring_view query,
	std::wstring_view replacement, SearchOptions const& options = {}) {
	if (query.empty())
		return std::wstring{text};
	auto const matches = FindSearchMatches(text, query, options);
	if (matches.empty())
		return std::wstring{text};
	std::wstring output;
	output.reserve(text.size());
	size_t copied = 0;
	for (auto const& match : matches) {
		output.append(text.substr(copied, match.position - copied));
		output.append(replacement);
		copied = match.position + match.length;
	}
	output.append(text.substr(copied));
	return output;
}

inline wchar_t SearchScopeLabel(SearchScope scope) {
	switch (scope) {
		case SearchScope::target: return L'C';
		case SearchScope::source: return L'O';
		case SearchScope::both: return L'B';
	}
	return L'B';
}

}
