#pragma once

#include "winui_bridge_text.h"
#include "winui_subtitle_qa.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace agi::winui {

struct RecentTranslationProject {
	std::string source_path;
	std::string target_path;
};

inline std::string SerializeRecentProjects(std::vector<RecentTranslationProject> const& projects,
	size_t maximum_count = 10) {
	std::ostringstream stream;
	stream << "AEGISUB-WINUI-RECENT\t2\n";
	auto const count = (std::min)(projects.size(), maximum_count);
	for (size_t index = 0; index < count; ++index) {
		stream << "PROJECT\t" << EscapeBridgeField(projects[index].source_path) << '\t'
			<< EscapeBridgeField(projects[index].target_path) << '\n';
	}
	return stream.str();
}

inline bool ParseRecentProjects(std::string_view input, std::vector<RecentTranslationProject>& projects) {
	projects.clear();
	std::istringstream stream{std::string{input}};
	std::string line;
	if (!std::getline(stream, line))
		return false;
	if (!line.empty() && line.back() == '\r')
		line.pop_back();

	if (line == "AEGISUB-WINUI-RECENT\t1") {
		RecentTranslationProject project;
		while (std::getline(stream, line)) {
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.rfind("SOURCE\t", 0) == 0)
				project.source_path = UnescapeBridgeField(line.substr(7));
			else if (line.rfind("TARGET\t", 0) == 0)
				project.target_path = UnescapeBridgeField(line.substr(7));
		}
		if (!project.source_path.empty() && !project.target_path.empty())
			projects.push_back(std::move(project));
		return !projects.empty();
	}

	if (line != "AEGISUB-WINUI-RECENT\t2")
		return false;
	while (std::getline(stream, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.rfind("PROJECT\t", 0) != 0)
			continue;
		auto const separator = line.find('\t', 8);
		if (separator == std::string::npos)
			return false;
		RecentTranslationProject project;
		project.source_path = UnescapeBridgeField(line.substr(8, separator - 8));
		project.target_path = UnescapeBridgeField(line.substr(separator + 1));
		if (!project.source_path.empty() && !project.target_path.empty())
			projects.push_back(std::move(project));
	}
	return true;
}

inline void TouchRecentProject(std::vector<RecentTranslationProject>& projects,
	RecentTranslationProject project, size_t maximum_count = 10) {
	projects.erase(std::remove_if(projects.begin(), projects.end(), [&](auto const& existing) {
		return existing.source_path == project.source_path && existing.target_path == project.target_path;
	}), projects.end());
	projects.insert(projects.begin(), std::move(project));
	if (projects.size() > maximum_count)
		projects.resize(maximum_count);
}

struct WinUiWorkspaceSettings {
	SubtitleQaSettings qa;
	bool autosave_draft{true};
	bool follow_system_theme{true};
	double editor_font_size{24.0};
	std::string ui_language{"cs-CZ"};
	std::string default_directory;
};

inline std::string SerializeWorkspaceSettings(WinUiWorkspaceSettings const& settings) {
	std::ostringstream stream;
	stream << "AEGISUB-WINUI-SETTINGS\t1\n";
	stream << "QA\t" << settings.qa.maximum_cpl << '\t' << settings.qa.maximum_cps << '\t'
		<< settings.qa.minimum_duration << '\t' << settings.qa.maximum_lines << '\n';
	stream << "RATIO\t" << settings.qa.minimum_length_ratio << '\t' << settings.qa.maximum_length_ratio << '\n';
	stream << "CHECKS\t" << settings.qa.check_terminal_punctuation << '\t'
		<< settings.qa.check_number_tokens << '\t' << settings.qa.check_spacing << '\t'
		<< settings.qa.check_repeated_punctuation << '\t' << settings.qa.check_length_ratio << '\t'
		<< settings.qa.check_czech_quotes << '\n';
	stream << "AUTOSAVE\t" << settings.autosave_draft << '\n';
	stream << "SYSTEM_THEME\t" << settings.follow_system_theme << '\n';
	stream << "FONT\t" << settings.editor_font_size << '\n';
	stream << "LANG\t" << EscapeBridgeField(settings.ui_language) << '\n';
	stream << "DIR\t" << EscapeBridgeField(settings.default_directory) << '\n';
	return stream.str();
}

inline bool ParseWorkspaceSettings(std::string_view input, WinUiWorkspaceSettings& settings) {
	WinUiWorkspaceSettings parsed;
	std::istringstream stream{std::string{input}};
	std::string line;
	if (!std::getline(stream, line) || line != "AEGISUB-WINUI-SETTINGS\t1")
		return false;
	try {
		while (std::getline(stream, line)) {
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.rfind("QA\t", 0) == 0) {
				std::istringstream values{line.substr(3)};
				values >> parsed.qa.maximum_cpl >> parsed.qa.maximum_cps >> parsed.qa.minimum_duration >> parsed.qa.maximum_lines;
				if (!values) return false;
			}
			else if (line.rfind("RATIO\t", 0) == 0) {
				std::istringstream values{line.substr(6)};
				values >> parsed.qa.minimum_length_ratio >> parsed.qa.maximum_length_ratio;
				if (!values) return false;
			}
			else if (line.rfind("CHECKS\t", 0) == 0) {
				std::istringstream values{line.substr(7)};
				values >> parsed.qa.check_terminal_punctuation >> parsed.qa.check_number_tokens >> parsed.qa.check_spacing
					>> parsed.qa.check_repeated_punctuation >> parsed.qa.check_length_ratio >> parsed.qa.check_czech_quotes;
				if (!values) return false;
			}
			else if (line.rfind("AUTOSAVE\t", 0) == 0) parsed.autosave_draft = line.substr(9) == "1";
			else if (line.rfind("SYSTEM_THEME\t", 0) == 0) parsed.follow_system_theme = line.substr(13) == "1";
			else if (line.rfind("FONT\t", 0) == 0) parsed.editor_font_size = std::stod(line.substr(5));
			else if (line.rfind("LANG\t", 0) == 0) parsed.ui_language = UnescapeBridgeField(line.substr(5));
			else if (line.rfind("DIR\t", 0) == 0) parsed.default_directory = UnescapeBridgeField(line.substr(4));
		}
	}
	catch (...) {
		return false;
	}
	if (parsed.qa.maximum_cpl == 0 || parsed.qa.maximum_lines == 0 || parsed.qa.maximum_cps <= 0.0 ||
		parsed.qa.minimum_duration < 0.0 || parsed.editor_font_size < 10.0 || parsed.editor_font_size > 72.0 ||
		parsed.qa.minimum_length_ratio <= 0.0 || parsed.qa.maximum_length_ratio < parsed.qa.minimum_length_ratio)
		return false;
	settings = std::move(parsed);
	return true;
}

}
