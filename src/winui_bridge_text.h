#pragma once

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

}
