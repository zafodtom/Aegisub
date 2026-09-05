#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace agi::winui {

enum class RecoveryArtifactKind {
	backup,
	draft,
};

struct RecoveryArtifactInfo {
	RecoveryArtifactKind kind{RecoveryArtifactKind::backup};
	std::wstring path;
	int64_t write_time{};
	uintmax_t size{};
	bool current_project{};
};

inline void SortRecoveryArtifacts(std::vector<RecoveryArtifactInfo>& artifacts) {
	std::stable_sort(artifacts.begin(), artifacts.end(), [](auto const& left, auto const& right) {
		if (left.current_project != right.current_project)
			return left.current_project;
		return left.write_time > right.write_time;
	});
}

inline std::vector<RecoveryArtifactInfo> RecoveryArtifactsForProject(
	std::vector<RecoveryArtifactInfo> artifacts, std::wstring const& project_key) {
	artifacts.erase(std::remove_if(artifacts.begin(), artifacts.end(), [&](auto const& artifact) {
		return artifact.path.find(project_key) == std::wstring::npos;
	}), artifacts.end());
	SortRecoveryArtifacts(artifacts);
	return artifacts;
}

}
