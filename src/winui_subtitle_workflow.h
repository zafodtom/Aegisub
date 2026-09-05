#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace agi::winui {

struct WorkflowRowState {
	bool translated{};
	bool modified{};
	bool ready{};
	bool approved{};
	bool has_problem{};
};

struct WorkflowSummary {
	size_t total{};
	size_t translated{};
	size_t modified{};
	size_t ready{};
	size_t approved{};
	size_t problems{};
	size_t remaining{};
	double completion_percent{};
};

inline WorkflowSummary SummarizeWorkflow(std::vector<WorkflowRowState> const& rows) {
	WorkflowSummary summary;
	summary.total = rows.size();
	for (auto const& row : rows) {
		summary.translated += row.translated ? 1 : 0;
		summary.modified += row.modified ? 1 : 0;
		summary.ready += row.ready && !row.has_problem ? 1 : 0;
		summary.approved += row.approved && !row.has_problem ? 1 : 0;
		summary.problems += row.has_problem ? 1 : 0;
	}
	summary.remaining = summary.total >= summary.approved ? summary.total - summary.approved : 0;
	summary.completion_percent = summary.total == 0 ? 0.0
		: 100.0 * static_cast<double>(summary.translated) / static_cast<double>(summary.total);
	return summary;
}

inline int32_t FindWorkflowRow(
	std::vector<WorkflowRowState> const& rows,
	int32_t current,
	int32_t direction,
	std::function<bool(WorkflowRowState const&)> const& predicate,
	bool wrap = true) {
	if (rows.empty() || direction == 0)
		return -1;
	auto const count = static_cast<int32_t>(rows.size());
	for (int32_t offset = 1; offset <= count; ++offset) {
		auto index = current + direction * offset;
		if (wrap) {
			index %= count;
			if (index < 0)
				index += count;
		}
		else if (index < 0 || index >= count) {
			break;
		}
		if (predicate(rows[static_cast<size_t>(index)]))
			return index;
	}
	return -1;
}

enum class SubtitlePairQuality {
	missing,
	weak,
	good,
	excellent,
	ambiguous,
};

inline SubtitlePairQuality AssessSubtitlePair(double overlap_quality, size_t candidate_count) {
	if (candidate_count == 0)
		return SubtitlePairQuality::missing;
	if (candidate_count > 1 && overlap_quality < 0.8)
		return SubtitlePairQuality::ambiguous;
	if (overlap_quality >= 0.85)
		return SubtitlePairQuality::excellent;
	if (overlap_quality >= 0.55)
		return SubtitlePairQuality::good;
	return SubtitlePairQuality::weak;
}

inline std::wstring SubtitlePairQualityLabel(SubtitlePairQuality quality) {
	switch (quality) {
		case SubtitlePairQuality::missing: return L"bez páru";
		case SubtitlePairQuality::weak: return L"slabá shoda";
		case SubtitlePairQuality::good: return L"dobrá shoda";
		case SubtitlePairQuality::excellent: return L"výborná shoda";
		case SubtitlePairQuality::ambiguous: return L"nejednoznačný pár";
	}
	return L"neznámá shoda";
}

inline bool PairingNeedsReview(SubtitlePairQuality quality) {
	return quality == SubtitlePairQuality::missing || quality == SubtitlePairQuality::weak ||
		quality == SubtitlePairQuality::ambiguous;
}

}
