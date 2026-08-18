#pragma once

#include <cwctype>
#include <limits>
#include <string>
#include <string_view>

namespace agi::winui {

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
