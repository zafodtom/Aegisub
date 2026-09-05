#pragma once

#include <string>
#include <vector>

namespace agi::winui {

struct SubtitleIoRow {
	std::wstring start;
	std::wstring end;
	double start_seconds{};
	double end_seconds{};
	double duration{};
	std::wstring text;
	std::wstring raw_text;
};

enum class SubtitleIoBackendKind {
	direct_library,
	bridge_process,
};

class SubtitleIoBackend {
public:
	virtual ~SubtitleIoBackend() = default;
	virtual SubtitleIoBackendKind kind() const noexcept = 0;
	virtual wchar_t const* name() const noexcept = 0;
	virtual bool read(std::wstring const& filename, std::vector<SubtitleIoRow>& rows,
		std::wstring& error_message) = 0;
	virtual bool write(std::wstring const& template_filename, std::wstring const& destination_filename,
		std::vector<SubtitleIoRow> const& rows, std::wstring& error_message) = 0;
};

struct SubtitleIoCapabilities {
	bool reads_srt{};
	bool reads_ass{};
	bool reads_ssa{};
	bool preserves_raw_formatting{};
	bool runs_in_process{};
};

inline SubtitleIoCapabilities BridgeSubtitleIoCapabilities() {
	return {true, true, true, true, false};
}

inline SubtitleIoCapabilities DirectLibrarySubtitleIoCapabilities() {
	return {true, true, true, true, true};
}

inline bool PreferDirectSubtitleIo(bool direct_backend_available) {
	return direct_backend_available;
}

// MainWindow should depend on SubtitleIoBackend rather than on the bridge protocol.
// The current bridge remains a compatibility backend until the Aegisub subtitle
// parser/writer is linked into the WinUI target. This interface makes that swap local
// instead of requiring another rewrite of the translation workflow.

}
