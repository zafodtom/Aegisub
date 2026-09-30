#include "pch.h"
#include "MainWindow.xaml.h"
#include "resource.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <shellapi.h>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace
{
    struct SingleCharacterEdit
    {
        int kind{};
        size_t position{};
    };

    SingleCharacterEdit ClassifySingleCharacterEdit(std::wstring_view before, std::wstring_view after)
    {
        int kind = 0;
        if (after.size() == before.size() + 1)
        {
            kind = 1;
        }
        else if (before.size() == after.size() + 1)
        {
            kind = 2;
        }
        else
        {
            return {};
        }

        auto const& shorter = kind == 1 ? before : after;
        auto const& longer = kind == 1 ? after : before;
        size_t position = 0;
        while (position < shorter.size() && shorter[position] == longer[position])
        {
            ++position;
        }
        if (shorter.substr(position) != longer.substr(position + 1))
        {
            return {};
        }
        return { kind, position };
    }

    size_t FindOrdinalIgnoreCase(std::wstring_view text, std::wstring_view query)
    {
        if (query.empty() || query.size() > text.size())
            return std::wstring_view::npos;

        for (size_t index = 0; index + query.size() <= text.size(); ++index)
        {
            if (CompareStringOrdinal(
                text.data() + index,
                static_cast<int>(query.size()),
                query.data(),
                static_cast<int>(query.size()),
                TRUE) == CSTR_EQUAL)
            {
                return index;
            }
        }
        return std::wstring_view::npos;
    }

    bool PathsReferToSameFile(std::wstring_view first, std::wstring_view second)
    {
        if (first.empty() || second.empty())
            return false;

        std::error_code equivalentError;
        if (std::filesystem::equivalent(
            std::filesystem::path(first), std::filesystem::path(second), equivalentError))
        {
            return true;
        }

        std::error_code firstError;
        std::error_code secondError;
        auto const firstPath = std::filesystem::absolute(
            std::filesystem::path(first), firstError).lexically_normal().wstring();
        auto const secondPath = std::filesystem::absolute(
            std::filesystem::path(second), secondError).lexically_normal().wstring();
        return !firstError && !secondError && _wcsicmp(firstPath.c_str(), secondPath.c_str()) == 0;
    }

    void ShowSameSubtitleFileWarning()
    {
        MessageBoxW(
            GetActiveWindow(),
            L"Origin\u00E1l a p\u0159eklad mus\u00ED b\u00FDt dva r\u016Fzn\u00E9 soubory.\n\n"
            L"Zvolen\u00FD soubor nebyl otev\u0159en, aby nemohlo doj\u00EDt k p\u0159eps\u00E1n\u00ED origin\u00E1lu.",
            L"Stejn\u00FD soubor nelze pou\u017E\u00EDt dvakr\u00E1t",
            MB_OK | MB_ICONWARNING);
    }

    std::filesystem::path WorkspaceStatePath(hstring const& targetPath)
    {
        if (targetPath.empty())
            return {};

        auto const required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (required == 0)
            return {};
        std::vector<wchar_t> localAppData(required);
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData.data(), required) == 0)
            return {};

        std::error_code pathError;
        auto normalized = std::filesystem::absolute(
            std::filesystem::path(targetPath.c_str()), pathError).lexically_normal().wstring();
        if (pathError)
            return {};
        if (!normalized.empty())
            CharLowerBuffW(normalized.data(), static_cast<DWORD>(normalized.size()));

        uint64_t hash = 1469598103934665603ULL;
        for (wchar_t character : normalized)
        {
            hash ^= static_cast<uint16_t>(character);
            hash *= 1099511628211ULL;
        }

        std::wostringstream filename;
        filename << std::hex << std::setfill(L'0') << std::setw(16) << hash << L".state";
        return std::filesystem::path(localAppData.data()) /
            L"SRTune" / L"TranslationWorkspace" / filename.str();
    }

    std::filesystem::path WorkspaceBackupPath(std::filesystem::path const& targetPath)
    {
        auto const statePath = WorkspaceStatePath(hstring{ targetPath.wstring() });
        if (statePath.empty())
            return {};

        auto filename = statePath.stem().wstring() + L"-" + targetPath.filename().wstring() + L".bak";
        return statePath.parent_path() / L"Backups" / filename;
    }

    std::filesystem::path WorkspaceDraftPath(hstring const& targetPath)
    {
        auto path = WorkspaceStatePath(targetPath);
        if (!path.empty())
            path.replace_extension(L".draft");
        return path;
    }

    std::filesystem::path WorkspaceRecentProjectPath()
    {
        auto const required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (required == 0)
            return {};
        std::vector<wchar_t> localAppData(required);
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData.data(), required) == 0)
            return {};
        return std::filesystem::path(localAppData.data()) /
            L"SRTune" / L"TranslationWorkspace" / L"last-project.tsv";
    }

    std::wstring FormatFileWriteTime(std::filesystem::path const& path)
    {
        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
            return {};

        FILETIME localFileTime{};
        SYSTEMTIME localSystemTime{};
        if (!FileTimeToLocalFileTime(&attributes.ftLastWriteTime, &localFileTime) ||
            !FileTimeToSystemTime(&localFileTime, &localSystemTime))
        {
            return {};
        }

        wchar_t date[80]{};
        wchar_t time[80]{};
        if (!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &localSystemTime,
                nullptr, date, static_cast<int>(std::size(date)), nullptr) ||
            !GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &localSystemTime,
                nullptr, time, static_cast<int>(std::size(time))))
        {
            return {};
        }
        return std::wstring{ date } + L" " + time;
    }

    void CleanupWorkspaceBackups(
        std::filesystem::path const& directory,
        std::filesystem::path const& currentBackup)
    {
        if (directory.filename() != L"Backups" ||
            directory.parent_path().filename() != L"TranslationWorkspace")
        {
            return;
        }

        struct BackupFile
        {
            std::filesystem::path path;
            std::filesystem::file_time_type writeTime;
        };
        std::vector<BackupFile> backups;
        auto const now = std::filesystem::file_time_type::clock::now();
        auto const tempMaxAge = std::chrono::hours(24);
        std::error_code error;

        std::filesystem::directory_iterator iterator(directory, error);
        std::filesystem::directory_iterator end;
        while (!error && iterator != end)
        {
            auto const entry = *iterator;
            iterator.increment(error);
            std::error_code entryError;
            if (!entry.is_regular_file(entryError) || entryError)
                continue;

            auto const writeTime = entry.last_write_time(entryError);
            if (entryError)
                continue;
            auto const filename = entry.path().filename().wstring();
            if (entry.path().extension() == L".bak")
            {
                backups.push_back({ entry.path(), writeTime });
            }
            else if (filename.find(L".bak.tmp-") != std::wstring::npos && now - writeTime > tempMaxAge)
            {
                std::filesystem::remove(entry.path(), entryError);
            }
        }

        std::sort(backups.begin(), backups.end(), [](auto const& first, auto const& second)
        {
            return first.writeTime > second.writeTime;
        });
        size_t otherRank = 0;
        for (auto const& backup : backups)
        {
            bool const current = backup.path == currentBackup;
            auto const ageHours = std::chrono::duration_cast<std::chrono::hours>(
                now - backup.writeTime).count();
            auto const rank = current ? size_t{} : otherRank++;
            if (!agi::winui::ShouldKeepRecoveryArtifact(current, ageHours, rank))
                std::filesystem::remove(backup.path, error);
        }
    }

    void CleanupWorkspaceDrafts(
        std::filesystem::path const& directory,
        std::filesystem::path const& currentDraft)
    {
        if (directory.filename() != L"TranslationWorkspace" ||
            directory.parent_path().filename() != L"SRTune")
        {
            return;
        }

        struct DraftFile
        {
            std::filesystem::path path;
            std::filesystem::file_time_type writeTime;
        };
        std::vector<DraftFile> drafts;
        auto const now = std::filesystem::file_time_type::clock::now();
        auto const tempMaxAge = std::chrono::hours(24);
        std::error_code error;

        std::filesystem::directory_iterator iterator(directory, error);
        std::filesystem::directory_iterator end;
        while (!error && iterator != end)
        {
            auto const entry = *iterator;
            iterator.increment(error);
            std::error_code entryError;
            if (!entry.is_regular_file(entryError) || entryError)
                continue;

            auto const writeTime = entry.last_write_time(entryError);
            if (entryError)
                continue;
            auto const filename = entry.path().filename().wstring();
            if (entry.path().extension() == L".draft")
            {
                drafts.push_back({ entry.path(), writeTime });
            }
            else if (filename.find(L".draft.tmp-") != std::wstring::npos &&
                now - writeTime > tempMaxAge)
            {
                std::filesystem::remove(entry.path(), entryError);
            }
        }

        std::sort(drafts.begin(), drafts.end(), [](auto const& first, auto const& second)
        {
            return first.writeTime > second.writeTime;
        });
        size_t otherRank = 0;
        for (auto const& draft : drafts)
        {
            bool const current = draft.path == currentDraft;
            auto const ageHours = std::chrono::duration_cast<std::chrono::hours>(
                now - draft.writeTime).count();
            auto const rank = current ? size_t{} : otherRank++;
            if (!agi::winui::ShouldKeepRecoveryArtifact(current, ageHours, rank))
                std::filesystem::remove(draft.path, error);
        }
    }

    bool FileFingerprint(std::filesystem::path const& path, uintmax_t& size, int64_t& timestamp)
    {
        std::error_code error;
        size = std::filesystem::file_size(path, error);
        if (error)
            return false;
        auto const writeTime = std::filesystem::last_write_time(path, error);
        if (error)
            return false;
        timestamp = static_cast<int64_t>(writeTime.time_since_epoch().count());
        return true;
    }

    std::filesystem::path FindBridgeFrom(std::filesystem::path start)
    {
        if (start.empty())
        {
            return {};
        }

        if (!std::filesystem::is_directory(start))
        {
            start = start.parent_path();
        }

        for (int depth = 0; depth < 12 && !start.empty(); ++depth)
        {
            auto const candidate = start / L"build" / L"src" / L"aegisub-winui-bridge.exe";
            if (std::filesystem::exists(candidate))
            {
                return candidate;
            }

            auto const parent = start.parent_path();
            if (parent == start)
            {
                break;
            }
            start = parent;
        }

        return {};
    }

    std::filesystem::path FindBridgeExecutable()
    {
        wchar_t modulePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath))) != 0)
        {
            auto const adjacent = std::filesystem::path(modulePath).parent_path() /
                L"aegisub-winui-bridge.exe";
            if (std::filesystem::exists(adjacent))
                return adjacent;
            if (auto const bridge = FindBridgeFrom(std::filesystem::path(modulePath)); !bridge.empty())
            {
                return bridge;
            }
        }

        std::error_code error;
        auto const current = std::filesystem::current_path(error);
        if (!error)
        {
            return FindBridgeFrom(current);
        }

        return {};
    }

    std::string EscapeBridgeField(std::string const& value)
    {
        std::string output;
        output.reserve(value.size());

        for (char c : value)
        {
            switch (c)
            {
            case '\\': output += "\\\\"; break;
            case '\t': output += "\\t"; break;
            case '\r': output += "\\r"; break;
            case '\n': output += "\\n"; break;
            default: output.push_back(c); break;
            }
        }

        return output;
    }

    std::string UnescapeBridgeField(std::string const& value)
    {
        std::string output;
        output.reserve(value.size());

        for (size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] != '\\' || i + 1 >= value.size())
            {
                output.push_back(value[i]);
                continue;
            }

            switch (value[++i])
            {
            case '\\': output.push_back('\\'); break;
            case 't': output.push_back('\t'); break;
            case 'r': output.push_back('\r'); break;
            case 'n': output.push_back('\n'); break;
            default:
                output.push_back('\\');
                output.push_back(value[i]);
                break;
            }
        }

        return output;
    }

    double TimestampSeconds(std::string const& value)
    {
        if (value.size() < 12)
        {
            return 0.0;
        }

        try
        {
            auto const hours = std::stoi(value.substr(0, 2));
            auto const minutes = std::stoi(value.substr(3, 2));
            auto const seconds = std::stod(value.substr(6));
            return hours * 3600.0 + minutes * 60.0 + seconds;
        }
        catch (...)
        {
            return 0.0;
        }
    }

    std::wstring ToWide(std::string const& value)
    {
        auto const converted = winrt::to_hstring(value);
        return std::wstring(converted.c_str());
    }

    bool RunProcess(std::wstring commandLine, DWORD& exitCode)
    {
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo{};

        if (!CreateProcessW(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo))
        {
            return false;
        }

        WaitForSingleObject(processInfo.hProcess, INFINITE);
        exitCode = 0;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return true;
    }

    bool ReadBridgeError(std::filesystem::path const& output, std::wstring& errorMessage)
    {
        std::ifstream stream(output, std::ios::binary);
        if (!stream)
        {
            return false;
        }

        std::string line;
        if (!std::getline(stream, line))
        {
            return false;
        }
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.rfind("ERROR\t", 0) != 0)
        {
            return false;
        }

        errorMessage = ToWide(UnescapeBridgeField(line.substr(6)));
        return true;
    }
}

    std::wstring TrimTranscriptText(std::wstring value)
    {
        auto const isSpace = [](wchar_t ch) { return std::iswspace(ch) != 0; };
        while (!value.empty() && isSpace(value.front()))
            value.erase(value.begin());
        while (!value.empty() && isSpace(value.back()))
            value.pop_back();
        return value;
    }

    std::wstring DecodeTextBytes(std::string bytes)
    {
        if (bytes.size() >= 2 &&
            static_cast<unsigned char>(bytes[0]) == 0xFF &&
            static_cast<unsigned char>(bytes[1]) == 0xFE)
        {
            std::wstring result;
            result.reserve((bytes.size() - 2) / 2);
            for (size_t index = 2; index + 1 < bytes.size(); index += 2)
            {
                auto const value = static_cast<wchar_t>(
                    static_cast<unsigned char>(bytes[index]) |
                    (static_cast<unsigned char>(bytes[index + 1]) << 8));
                result.push_back(value);
            }
            return result;
        }

        if (bytes.size() >= 3 &&
            static_cast<unsigned char>(bytes[0]) == 0xEF &&
            static_cast<unsigned char>(bytes[1]) == 0xBB &&
            static_cast<unsigned char>(bytes[2]) == 0xBF)
        {
            bytes.erase(0, 3);
        }

        try
        {
            return std::wstring{ winrt::to_hstring(bytes).c_str() };
        }
        catch (...)
        {
            if (bytes.empty())
                return {};
            auto const required = MultiByteToWideChar(
                CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
            if (required <= 0)
                return {};
            std::wstring result(static_cast<size_t>(required), L'\0');
            MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()),
                result.data(), required);
            return result;
        }
    }

    std::wstring DecodeXmlEntities(std::wstring value)
    {
        auto replaceAll = [&value](std::wstring_view from, std::wstring_view to)
        {
            size_t position = 0;
            while ((position = value.find(from, position)) != std::wstring::npos)
            {
                value.replace(position, from.size(), to);
                position += to.size();
            }
        };
        replaceAll(L"&amp;", L"&");
        replaceAll(L"&lt;", L"<");
        replaceAll(L"&gt;", L">");
        replaceAll(L"&quot;", L"\"");
        replaceAll(L"&apos;", L"'");
        replaceAll(L"&#10;", L"\n");
        replaceAll(L"&#13;", L"\r");
        return value;
    }

    std::vector<winrt::hstring> ChunkTranscriptText(std::wstring text)
    {
        for (auto& ch : text)
        {
            if (ch == L'\r')
                ch = L'\n';
            else if (ch == L'\t')
                ch = L' ';
        }

        std::vector<std::wstring> paragraphs;
        std::wistringstream stream(text);
        std::wstring line;
        std::wstring paragraph;

        auto flushParagraph = [&]()
        {
            auto cleaned = TrimTranscriptText(paragraph);
            if (!cleaned.empty())
                paragraphs.push_back(std::move(cleaned));
            paragraph.clear();
        };

        while (std::getline(stream, line))
        {
            line = TrimTranscriptText(line);
            if (line.empty())
            {
                flushParagraph();
                continue;
            }
            if (!paragraph.empty())
                paragraph += L' ';
            paragraph += line;
        }
        flushParagraph();

        if (paragraphs.empty())
        {
            auto cleaned = TrimTranscriptText(text);
            if (!cleaned.empty())
                paragraphs.push_back(std::move(cleaned));
        }

        std::vector<winrt::hstring> chunks;
        for (auto const& source : paragraphs)
        {
            size_t offset = 0;
            while (offset < source.size())
            {
                auto const remaining = source.size() - offset;
                if (remaining <= 700)
                {
                    auto piece = TrimTranscriptText(source.substr(offset));
                    if (!piece.empty())
                        chunks.emplace_back(piece);
                    break;
                }

                size_t cut = offset + 620;
                auto const lowerBound = offset + 360;
                auto const upperBound = (std::min)(source.size(), offset + 700);
                for (size_t cursor = upperBound; cursor > lowerBound; --cursor)
                {
                    auto const ch = source[cursor - 1];
                    if (ch == L'.' || ch == L'?' || ch == L'!' || ch == L';')
                    {
                        cut = cursor;
                        break;
                    }
                    if (cursor <= offset + 620 && std::iswspace(ch))
                    {
                        cut = cursor;
                        break;
                    }
                }

                auto piece = TrimTranscriptText(source.substr(offset, cut - offset));
                if (!piece.empty())
                    chunks.emplace_back(piece);
                offset = cut;
            }
        }

        return chunks;
    }

    std::set<std::wstring> TranscriptWords(std::wstring_view text)
    {
        std::set<std::wstring> words;
        std::wstring current;
        for (wchar_t ch : text)
        {
            if (std::iswalnum(ch))
            {
                current.push_back(static_cast<wchar_t>(std::towlower(ch)));
            }
            else
            {
                if (current.size() >= 3)
                    words.insert(current);
                current.clear();
            }
        }
        if (current.size() >= 3)
            words.insert(current);
        return words;
    }

    double TranscriptMatchScore(std::wstring_view reference, std::wstring_view candidate)
    {
        auto const referenceWords = TranscriptWords(reference);
        auto const candidateWords = TranscriptWords(candidate);
        if (referenceWords.empty() || candidateWords.empty())
            return 0.0;

        size_t overlap = 0;
        for (auto const& word : referenceWords)
        {
            if (candidateWords.find(word) != candidateWords.end())
                ++overlap;
        }
        return static_cast<double>(overlap) /
            std::sqrt(static_cast<double>(referenceWords.size() * candidateWords.size()));
    }

    std::wstring PowerShellQuoted(std::wstring value)
    {
        size_t position = 0;
        while ((position = value.find(L'\'', position)) != std::wstring::npos)
        {
            value.insert(position, 1, L'\'');
            position += 2;
        }
        return L"'" + value + L"'";
    }

    std::wstring StoreProjectPath(
        std::filesystem::path const& projectFile,
        std::wstring_view value)
    {
        if (value.empty())
            return {};

        std::error_code absoluteError;
        auto const absolute = std::filesystem::absolute(
            std::filesystem::path(value), absoluteError).lexically_normal();
        if (absoluteError)
            return std::wstring{ value };

        std::error_code relativeError;
        auto const relative = std::filesystem::relative(
            absolute, projectFile.parent_path(), relativeError);
        if (!relativeError && !relative.empty())
            return relative.generic_wstring();

        return absolute.wstring();
    }

    std::wstring ResolveProjectPath(
        std::filesystem::path const& projectFile,
        std::wstring_view value)
    {
        if (value.empty())
            return {};

        std::filesystem::path path{ value };
        if (path.is_relative())
            path = projectFile.parent_path() / path;

        std::error_code error;
        auto absolute = std::filesystem::absolute(path, error);
        return error ? path.lexically_normal().wstring() : absolute.lexically_normal().wstring();
    }


namespace winrt::SRTune::implementation
{
    int32_t MainWindow::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MainWindow::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void MainWindow::RootGrid_Loaded(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_initialized)
        {
            return;
        }

        m_initialized = true;

        // WinUI does not reliably propagate ApplicationIcon to an unpackaged window.
        // Set both the AppWindow icon and the native Win32 window icons explicitly.
        wchar_t modulePath[32768]{};
        auto const modulePathLength = GetModuleFileNameW(
            nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));
        if (modulePathLength && modulePathLength < std::size(modulePath))
        {
            auto const iconPath = std::filesystem::path{ modulePath }.parent_path() / L"SRTune.ico";
            if (std::filesystem::exists(iconPath))
                AppWindow().SetIcon(iconPath.wstring());
        }

        HWND hwnd{};
        if (auto const windowNative = this->try_as<::IWindowNative>();
            windowNative && SUCCEEDED(windowNative->get_WindowHandle(&hwnd)) && hwnd)
        {
            auto const instance = GetModuleHandleW(nullptr);
            auto const bigIcon = reinterpret_cast<HICON>(LoadImageW(
                instance, MAKEINTRESOURCEW(IDI_SRTUNE_ICON), IMAGE_ICON,
                GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
            auto const smallIcon = reinterpret_cast<HICON>(LoadImageW(
                instance, MAKEINTRESOURCEW(IDI_SRTUNE_ICON), IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));

            if (bigIcon)
                SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
            if (smallIcon)
                SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
        }

        InitializeDynamicSubtitleGrid();
        if (m_selectedSubtitleIndices.empty() && !m_rows.empty())
        {
            m_selectedSubtitleIndices.push_back(m_currentIndex);
            m_selectionAnchorIndex = m_currentIndex;
        }
        RebuildSubtitleGrid();
        HookWindowClosing();
        StartExternalChangeMonitoring();
        StartMediaUiTimer();
        LoadLastGlossaryFile();
        RefreshRecentProjectAction();
        if (m_rows.empty())
            RefreshEmptyWorkspaceUi();
        else
            LoadCurrentRow();
        RefreshProjectFileLabels();
        RefreshSearchSummary();
    }

    void MainWindow::PreviousButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_rows.empty())
            return;

        MoveCurrentBy(-1);
    }

    void MainWindow::NextButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_rows.empty())
            return;

        MoveCurrentBy(1);
    }

    void MainWindow::ApproveButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_rows.empty())
            return;

        ClearBulkUndo();

        auto& row = m_rows[m_currentIndex];
        if (row.workflowStatus == L"Schv\u00E1leno")
        {
            row.workflowStatus = L"P\u0159ipraveno";
            row.status = row.workflowStatus;
            if (!row.targetModified)
                row.savedWorkflowStatus = row.workflowStatus;
            m_workflowStateDirty = true;
            UpdateDirtyFromRows();
            RefreshQaAll();
            UpdateTableRow(m_currentIndex);
            RefreshCurrentQaVisuals();
            StatusBarText().Text(L"Titulek vr\u00E1cen ke kontrole");
            TargetTextBox().Focus(FocusState::Programmatic);
            return;
        }

        CommitCurrentAndMoveNext(true);
    }

    void MainWindow::SaveButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        SaveFromShortcut();
    }

    void MainWindow::SaveAsButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        SaveAsFromShortcut();
    }

    void MainWindow::RestoreBackupButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_targetPath.empty() || !ConfirmSaveBefore(L"obnoven\u00EDm z\u00E1lohy"))
            return;

        auto const targetPath = std::filesystem::path(m_targetPath.c_str());
        auto const backupPath = WorkspaceBackupPath(targetPath);
        if (backupPath.empty() || !std::filesystem::exists(backupPath))
        {
            RefreshBackupAction();
            MessageBoxW(GetActiveWindow(), L"Pro aktu\u00E1ln\u00ED soubor p\u0159ekladu nebyla nalezena z\u00E1loha.",
                L"Z\u00E1loha nen\u00ED k dispozici", MB_OK | MB_ICONINFORMATION);
            return;
        }

        std::wstring confirmation = L"Opravdu chcete obnovit p\u0159edchoz\u00ED ulo\u017Eenou verzi?";
        auto const backupTime = FormatFileWriteTime(backupPath);
        if (!backupTime.empty())
            confirmation += L"\n\nZ\u00E1loha: " + backupTime;
        confirmation += L"\n\nSou\u010Dasn\u00E1 verze se zachov\u00E1 jako nov\u00E1 z\u00E1loha.";
        if (MessageBoxW(GetActiveWindow(), confirmation.c_str(), L"Obnovit z\u00E1lohu?",
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
        {
            return;
        }

        auto validationPath = std::filesystem::temp_directory_path() /
            (L"srtune-restore-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(GetTickCount64()) + targetPath.extension().wstring());
        std::error_code fileError;
        std::filesystem::remove(validationPath, fileError);
        if (!CopyFileW(backupPath.c_str(), validationPath.c_str(), FALSE))
        {
            MessageBoxW(GetActiveWindow(), L"Z\u00E1lohu se nepoda\u0159ilo p\u0159ipravit ke kontrole.",
                L"Obnova se nezda\u0159ila", MB_OK | MB_ICONERROR);
            return;
        }

        std::vector<SubtitleEntry> restoredEntries;
        std::wstring errorMessage;
        bool const validBackup = ReadSubtitleFile(validationPath.wstring(), restoredEntries, errorMessage);
        std::filesystem::remove(validationPath, fileError);
        if (!validBackup)
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Z\u00E1loha nen\u00ED platn\u00FD soubor titulk\u016F",
                MB_OK | MB_ICONERROR);
            return;
        }

        auto currentTemp = backupPath;
        currentTemp += L".current-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
        auto restoredTemp = targetPath.parent_path() /
            (targetPath.filename().wstring() + L".restore-" + std::to_wstring(GetCurrentProcessId()) + L".tmp");
        std::filesystem::remove(currentTemp, fileError);
        std::filesystem::remove(restoredTemp, fileError);
        if (!CopyFileW(targetPath.c_str(), currentTemp.c_str(), FALSE) ||
            !CopyFileW(backupPath.c_str(), restoredTemp.c_str(), FALSE))
        {
            std::filesystem::remove(currentTemp, fileError);
            std::filesystem::remove(restoredTemp, fileError);
            MessageBoxW(GetActiveWindow(), L"Nepoda\u0159ilo se bezpe\u010Dn\u011B p\u0159ipravit v\u00FDm\u011Bnu soubor\u016F. P\u016Fvodn\u00ED soubor nebyl zm\u011Bn\u011Bn.",
                L"Obnova se nezda\u0159ila", MB_OK | MB_ICONERROR);
            return;
        }

        if (!MoveFileExW(restoredTemp.c_str(), targetPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(currentTemp, fileError);
            std::filesystem::remove(restoredTemp, fileError);
            MessageBoxW(GetActiveWindow(), L"Z\u00E1lohu se nepoda\u0159ilo obnovit. P\u016Fvodn\u00ED soubor nebyl zm\u011Bn\u011Bn.",
                L"Obnova se nezda\u0159ila", MB_OK | MB_ICONERROR);
            return;
        }
        if (!MoveFileExW(currentTemp.c_str(), backupPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(restoredTemp, fileError);
            if (CopyFileW(currentTemp.c_str(), targetPath.c_str(), FALSE))
            {
                std::filesystem::remove(currentTemp, fileError);
                MessageBoxW(GetActiveWindow(), L"Nepoda\u0159ilo se zachovat sou\u010Dasnou verzi jako novou z\u00E1lohu. Obnova byla bezpe\u010Dn\u011B vr\u00E1cena zp\u011Bt.",
                    L"Obnova se nezda\u0159ila", MB_OK | MB_ICONERROR);
            }
            else
            {
                std::wstring message = L"Obnoven\u00E1 verze je nyn\u00ED otev\u0159en\u00E1. P\u016Fvodn\u00ED verzi se nepoda\u0159ilo vr\u00E1tit, ale jej\u00ED bezpe\u010Dn\u00E1 kopie z\u016Fstala zde:\n\n";
                message += currentTemp.wstring();
                MessageBoxW(GetActiveWindow(), message.c_str(), L"Obnova vy\u017Eaduje pozornost", MB_OK | MB_ICONWARNING);
            }
            return;
        }

        std::filesystem::last_write_time(backupPath, std::filesystem::file_time_type::clock::now(), fileError);
        m_targetEntries = std::move(restoredEntries);
        RefreshLoadedProject();
        StatusBarText().Text(L"P\u0159edchoz\u00ED verze obnovena \u00B7 p\u016Fvodn\u00ED verze je nyn\u00ED z\u00E1loha");
    }

    void MainWindow::RefreshEmptyWorkspaceUi()
    {
        m_loadingSelection = true;
        HeaderCurrentSubtitleText().Text(L"");
        OriginalTimingText().Text(L"");
        OriginalTextBox().Text(L"");
        TargetInfoText().Text(L"");
        StartTimeBox().Text(L"");
        EndTimeBox().Text(L"");
        TimingDurationText().Text(L"");
        TargetTextBox().Text(L"");
        TargetCplText().Text(L"");
        TargetCpsText().Text(L"");
        TargetLengthText().Text(L"");
        TargetProblemText().Text(L"");
        TargetProblemText().Visibility(Visibility::Collapsed);
        TargetStatusText().Text(L"");
        TablePositionText().Text(L"");
        SubtitleSelectionText().Text(L"0 titulků");
        TranscriptPrev3Block().Visibility(Visibility::Collapsed);
        TranscriptPrev2Block().Visibility(Visibility::Collapsed);
        TranscriptPreviousBlock().Visibility(Visibility::Collapsed);
        TranscriptNextBlock().Visibility(Visibility::Collapsed);
        TranscriptNext2Block().Visibility(Visibility::Collapsed);
        TranscriptNext3Block().Visibility(Visibility::Collapsed);
        TranscriptCurrentTimeText().Text(L"");
        TranscriptCurrentText().Text(L"");
        m_loadingSelection = false;
    }

    void MainWindow::ResetWorkspaceToBlank()
    {
        m_sourceEntries.clear();
        m_targetEntries.clear();
        m_transcriptEntries.clear();
        m_transcriptChunks.clear();
        m_rows.clear();
        m_sourcePath = L"";
        m_targetPath = L"";
        m_transcriptPath = L"";
        m_projectPath.clear();
        m_currentIndex = 0;
        m_selectedSubtitleIndices.clear();
        m_selectionAnchorIndex = -1;
        m_originalPanelManuallyHidden = false;
        m_structureDirty = false;
        m_workflowStateDirty = false;
        m_hasTargetFileFingerprint = false;
        m_forceSaveAsForRecoveredDraft = false;
        m_externalChangeAcknowledged = false;
        ClearBulkUndo();
        ClearWorkspaceHistory();
        CloseVideoFile();

        m_waveformPath.clear();
        m_waveformPeaks.clear();
        m_waveformDuration = 0.0;
        m_waveformWindowStart = 0.0;
        m_waveformWindowEnd = 0.0;
        m_waveformViewportSubtitleIndex = -1;
        WaveformFileText().Text(L"");
        WaveformRangeText().Text(L"");
        WaveformCanvas().Children().Clear();
        WholeTimelineCanvas().Children().Clear();
        m_waveformActiveSelection = nullptr;
        m_waveformActiveStartMarker = nullptr;
        m_waveformActiveEndMarker = nullptr;
        m_waveformPlayhead = nullptr;
        m_wholeTimelineViewport = nullptr;

        m_timelineSliderUpdating = true;
        TimelineSlider().Minimum(0.0);
        TimelineSlider().Maximum(1.0);
        TimelineSlider().Value(0.0);
        m_timelineSliderUpdating = false;

        SearchBar().Visibility(Visibility::Collapsed);
        RebuildSubtitleGrid();
        RefreshEmptyWorkspaceUi();
        RefreshProjectFileLabels();
        SetDirty(false);
        StatusBarText().Text(L"Nový prázdný projekt");
    }

    void MainWindow::NewProjectMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (!ConfirmSaveBefore(L"zahájením nového projektu"))
            return;
        ResetWorkspaceToBlank();
    }

    void MainWindow::OpenProjectMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        OpenProjectFile();
    }

    void MainWindow::SaveProjectMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        SaveProjectFile(false);
    }

    void MainWindow::SaveProjectAsMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        SaveProjectFile(true);
    }

    void MainWindow::CloseSourceMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_sourcePath.empty() && m_sourceEntries.empty())
            return;

        m_sourceEntries.clear();
        m_sourcePath = L"";
        m_originalPanelManuallyHidden = false;
        for (auto& row : m_rows)
        {
            row.original = L"";
            row.sourceStart = L"";
            row.sourceEnd = L"";
            row.sourceMatchQuality = 0.0;
            row.manualSourceIndex = -1;
            row.pairingIgnored = false;
        }

        RefreshProjectFileLabels();
        RebuildSubtitleGrid();
        if (m_rows.empty())
            RefreshEmptyWorkspaceUi();
        else
            LoadCurrentRow();
        RenderWaveform();
        StatusBarText().Text(L"Originál zavřen");
    }

    void MainWindow::CloseTargetMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_targetPath.empty() && m_rows.empty())
            return;
        if (!ConfirmSaveBefore(L"zavřením překladu"))
            return;

        m_targetEntries.clear();
        m_targetPath = L"";
        m_hasTargetFileFingerprint = false;
        m_forceSaveAsForRecoveredDraft = false;
        m_structureDirty = false;
        m_workflowStateDirty = false;

        BuildAlignedRows();
        m_selectedSubtitleIndices.clear();
        if (!m_rows.empty())
            m_selectedSubtitleIndices.push_back(0);
        m_selectionAnchorIndex = m_rows.empty() ? -1 : 0;
        RebuildSubtitleGrid();
        RefreshProjectFileLabels();
        if (m_rows.empty())
            RefreshEmptyWorkspaceUi();
        else
            LoadCurrentRow();
        SetDirty(false);
        StatusBarText().Text(L"Překlad zavřen");
    }

    void MainWindow::CloseVideoMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        CloseVideoFile();
        StatusBarText().Text(L"Video zavřeno");
    }

    void MainWindow::SwapSourceTargetMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_sourceEntries.empty() || m_targetEntries.empty() ||
            m_sourcePath.empty() || m_targetPath.empty())
        {
            StatusBarText().Text(L"Pro přehození musí být načten originál i překlad");
            return;
        }

        if (!ConfirmSaveBefore(L"přehozením originálu a překladu"))
            return;

        std::swap(m_sourceEntries, m_targetEntries);
        std::swap(m_sourcePath, m_targetPath);
        m_originalPanelManuallyHidden = false;
        RefreshLoadedProject();
        StatusBarText().Text(L"Originál a překlad byly přehozeny");
    }

    void MainWindow::OpenBothButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        OpenProjectFiles();
    }

    void MainWindow::OpenSourceButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        OpenSourceFile();
    }

    void MainWindow::OpenTargetButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        OpenTargetFile();
    }

    void MainWindow::OpenTranscriptButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        OpenTranscriptFile();
    }

    void MainWindow::CloseTranscriptMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_transcriptEntries.empty() && m_transcriptPath.empty())
            return;

        m_transcriptEntries.clear();
        m_transcriptChunks.clear();
        m_transcriptPath = L"";
        RefreshProjectFileLabels();
        RefreshTranscriptContext();
        StatusBarText().Text(m_sourceEntries.empty()
            ? L"Samostatn\u00FD transcript zav\u0159en \u00B7 kontext nem\u00E1 zdroj"
            : L"Samostatn\u00FD transcript zav\u0159en \u00B7 kontext se op\u011Bt bere z origin\u00E1lu");
    }

    void MainWindow::OpenRecentProjectButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        OpenRecentProject();
    }

    void MainWindow::RecoveryOverviewButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_targetPath.empty())
        {
            MessageBoxW(GetActiveWindow(), L"Nejd\u0159\u00EDve otev\u0159ete soubor p\u0159ekladu.",
                L"Obnovovac\u00ED data", MB_OK | MB_ICONINFORMATION);
            return;
        }

        auto describe = [](std::filesystem::path const& path, wchar_t const* missing)
        {
            std::error_code error;
            if (path.empty() || !std::filesystem::exists(path, error))
                return std::wstring{ missing };
            auto const size = std::filesystem::file_size(path, error);
            std::wstring result = path.filename().wstring() + L"\n  " + FormatFileWriteTime(path);
            if (!error)
                result += L" \u00B7 " + std::to_wstring(size) + L" B";
            result += L"\n  " + path.wstring();
            return result;
        };

        auto const targetPath = std::filesystem::path(m_targetPath.c_str());
        std::wstring message = L"Z\u00E1loha:\n" +
            describe(WorkspaceBackupPath(targetPath), L"nen\u00ED k dispozici") +
            L"\n\nPracovn\u00ED koncept:\n" +
            describe(WorkspaceDraftPath(m_targetPath), L"nen\u00ED k dispozici");
        MessageBoxW(GetActiveWindow(), message.c_str(), L"P\u0159ehled obnovovac\u00EDch dat",
            MB_OK | MB_ICONINFORMATION);
    }

    void MainWindow::OpenRecoveryFolderButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        auto const recentPath = WorkspaceRecentProjectPath();
        if (recentPath.empty())
            return;
        std::error_code error;
        std::filesystem::create_directories(recentPath.parent_path() / L"Backups", error);
        auto const folder = recentPath.parent_path();
        if (reinterpret_cast<INT_PTR>(ShellExecuteW(
                GetActiveWindow(), L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
        {
            MessageBoxW(GetActiveWindow(), L"Syst\u00E9movou slo\u017Eku obnovy se nepoda\u0159ilo otev\u0159\u00EDt.",
                L"Obnovovac\u00ED data", MB_OK | MB_ICONERROR);
        }
    }

    void MainWindow::DeleteRecoveryFilesButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_targetPath.empty())
            return;
        if (MessageBoxW(GetActiveWindow(),
                L"Odstranit z\u00E1lohu a pracovn\u00ED koncept aktu\u00E1ln\u00EDho souboru p\u0159ekladu?\n\n"
                L"Ulo\u017Een\u00E9 titulky z\u016Fstanou beze zm\u011Bny.",
                L"Odstranit obnovovac\u00ED data?", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        {
            return;
        }
        std::error_code error;
        std::filesystem::remove(WorkspaceBackupPath(std::filesystem::path(m_targetPath.c_str())), error);
        DeleteWorkspaceDraft();
        RefreshBackupAction();
        StatusBarText().Text(L"Obnovovac\u00ED data aktu\u00E1ln\u00EDho souboru odstran\u011Bna");
        if (m_hasUnsavedChanges)
            ScheduleWorkspaceDraftSave();
    }

    void MainWindow::SearchTextBox_TextChanged(
        Windows::Foundation::IInspectable const&,
        TextChangedEventArgs const&)
    {
        RefreshSearchSummary();
    }

    void MainWindow::SearchTextBox_KeyDown(
        Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        if (args.Key() == Windows::System::VirtualKey::Enter)
        {
            args.Handled(true);
            bool const shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            MoveToSearchResult(shift ? -1 : 1);
        }
        else if (args.Key() == Windows::System::VirtualKey::Escape)
        {
            args.Handled(true);
            TargetTextBox().Focus(FocusState::Programmatic);
        }
    }

    void MainWindow::SearchPreviousButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        MoveToSearchResult(-1);
    }

    void MainWindow::SearchNextButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        MoveToSearchResult(1);
    }

    void MainWindow::FilterComboBox_SelectionChanged(
        Windows::Foundation::IInspectable const&,
        SelectionChangedEventArgs const&)
    {
        auto const selected = FilterComboBox().SelectedIndex();
        if (selected < 0 || selected > static_cast<int32_t>(agi::winui::SubtitleFilter::approved))
            return;

        m_activeFilter = static_cast<agi::winui::SubtitleFilter>(selected);
        if (!m_initialized)
            return;
        RefreshProgressSummary();
        RefreshSearchSummary();
        UpdateSelectionVisuals();
        if (m_currentIndex >= 0 && m_currentIndex < static_cast<int32_t>(m_rows.size()) &&
            RowMatchesActiveFilter(m_rows[m_currentIndex]))
        {
            ScrollCurrentRowIntoView();
        }
    }

    std::vector<size_t> MainWindow::VisibleRowIndices() const
    {
        std::vector<size_t> indices;
        for (size_t index = 0; index < m_rows.size(); ++index)
        {
            if (RowMatchesActiveFilter(m_rows[index]))
                indices.push_back(index);
        }
        return indices;
    }

    size_t MainWindow::ReplacementCount(std::wstring_view query) const
    {
        size_t count = 0;
        for (auto const index : VisibleRowIndices())
        {
            auto const& target = m_rows[index].target;
            count += agi::winui::CountCaseInsensitiveMatches(
                std::wstring_view{ target.c_str(), target.size() }, query);
        }
        return count;
    }

    void MainWindow::ReplacePreviewButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        std::wstring const query{ SearchTextBox().Text().c_str() };
        if (query.empty())
        {
            MessageBoxW(GetActiveWindow(), L"Nejd\u0159\u00EDve zadejte hledan\u00FD text.",
                L"N\u00E1hled nahrazen\u00ED", MB_OK | MB_ICONINFORMATION);
            SearchTextBox().Focus(FocusState::Programmatic);
            return;
        }

        size_t occurrenceCount = 0;
        size_t rowCount = 0;
        std::wstring details;
        for (auto const index : VisibleRowIndices())
        {
            auto const& row = m_rows[index];
            auto const matches = agi::winui::CountCaseInsensitiveMatches(
                std::wstring_view{ row.target.c_str(), row.target.size() }, query);
            if (matches == 0)
                continue;
            occurrenceCount += matches;
            ++rowCount;
            if (rowCount <= 12)
                details += L"\n#" + std::to_wstring(row.number) + L" \u00B7 " + std::to_wstring(matches) + L"\u00D7";
        }
        if (rowCount > 12)
            details += L"\n\u2026 a dal\u0161\u00EDch " + std::to_wstring(rowCount - 12) + L" titulk\u016F";

        std::wstring message = occurrenceCount == 0
            ? L"Ve zobrazen\u00E9m p\u0159ekladu nebyla nalezena \u017E\u00E1dn\u00E1 shoda."
            : L"Nalezeno " + std::to_wstring(occurrenceCount) + L" v\u00FDskyt\u016F v " +
                std::to_wstring(rowCount) + L" titulc\u00EDch." + details;
        MessageBoxW(GetActiveWindow(), message.c_str(), L"N\u00E1hled nahrazen\u00ED",
            MB_OK | (occurrenceCount == 0 ? MB_ICONINFORMATION : MB_ICONASTERISK));
    }

    void MainWindow::CaptureBulkSnapshot(std::vector<size_t> const& indices, hstring const& action)
    {
        CaptureWorkspaceUndoSnapshot(action);
        m_lastBulkWorkflowStateDirty = m_workflowStateDirty;
        m_lastBulkSnapshot.clear();
        m_lastBulkSnapshot.reserve(indices.size());
        for (auto const index : indices)
        {
            if (index < m_rows.size())
                m_lastBulkSnapshot.push_back({ index, m_rows[index].target, m_rows[index].workflowStatus });
        }
        m_lastBulkAction = action;
        UndoBulkButton().IsEnabled(!m_lastBulkSnapshot.empty());
        UndoBulkButton().Content(box_value(hstring{ L"Vr\u00E1tit: " + std::wstring{ action.c_str() } }));
    }

    void MainWindow::ClearBulkUndo()
    {
        m_lastBulkSnapshot.clear();
        m_lastBulkAction.clear();
        m_lastBulkWorkflowStateDirty = false;
        if (m_initialized)
        {
            UndoBulkButton().IsEnabled(false);
            UndoBulkButton().Content(box_value(hstring{ L"Vr\u00E1tit hromadnou zm\u011Bnu" }));
        }
    }

    void MainWindow::ReplaceAllButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        std::wstring const query{ SearchTextBox().Text().c_str() };
        std::wstring const replacement{ ReplaceTextBox().Text().c_str() };
        auto const occurrences = ReplacementCount(query);
        if (query.empty() || occurrences == 0)
        {
            ReplacePreviewButton_Click(nullptr, nullptr);
            return;
        }

        std::vector<size_t> affected;
        for (auto const index : VisibleRowIndices())
        {
            auto const& target = m_rows[index].target;
            if (agi::winui::CountCaseInsensitiveMatches(
                    std::wstring_view{ target.c_str(), target.size() }, query) > 0)
            {
                affected.push_back(index);
            }
        }
        std::wstring message = L"Nahradit " + std::to_wstring(occurrences) + L" v\u00FDskyt\u016F v " +
            std::to_wstring(affected.size()) + L" zobrazen\u00FDch titulc\u00EDch?";
        if (MessageBoxW(GetActiveWindow(), message.c_str(), L"Hromadn\u00E9 nahrazen\u00ED",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        {
            return;
        }

        CaptureBulkSnapshot(affected, L"nahrazen\u00ED textu");
        for (auto const index : affected)
        {
            auto& row = m_rows[index];
            row.target = hstring{ agi::winui::ReplaceCaseInsensitive(
                std::wstring_view{ row.target.c_str(), row.target.size() }, query, replacement) };
            row.targetModified = !agi::winui::EquivalentEditorText(row.target.c_str(), row.savedTarget.c_str());
            if (row.targetModified)
                row.workflowStatus = L"Upraveno";
            if (index < m_targetEntries.size())
                m_targetEntries[index].text = row.target;
        }
        m_workflowStateDirty = true;
        UpdateDirtyFromRows();
        RefreshQaAll();
        LoadCurrentRow();
        ScheduleWorkspaceDraftSave();
        StatusBarText().Text(hstring{ L"Nahrazeno " + std::to_wstring(occurrences) +
            L" v\u00FDskyt\u016F \u00B7 operaci lze vr\u00E1tit" });
    }

    void MainWindow::RestoreLastBulkSnapshot()
    {
        if (m_lastBulkSnapshot.empty())
            return;
        auto const action = std::wstring{ m_lastBulkAction.c_str() };
        for (auto const& snapshot : m_lastBulkSnapshot)
        {
            if (snapshot.index >= m_rows.size())
                continue;
            auto& row = m_rows[snapshot.index];
            row.target = snapshot.target;
            row.workflowStatus = snapshot.workflowStatus;
            row.targetModified = !agi::winui::EquivalentEditorText(row.target.c_str(), row.savedTarget.c_str());
            if (snapshot.index < m_targetEntries.size())
                m_targetEntries[snapshot.index].text = row.target;
        }
        m_lastBulkSnapshot.clear();
        m_lastBulkAction.clear();
        UndoBulkButton().IsEnabled(false);
        UndoBulkButton().Content(box_value(hstring{ L"Vr\u00E1tit hromadnou zm\u011Bnu" }));
        m_workflowStateDirty = m_lastBulkWorkflowStateDirty;
        UpdateDirtyFromRows();
        RefreshQaAll();
        LoadCurrentRow();
        ScheduleWorkspaceDraftSave();
        StatusBarText().Text(hstring{ L"Hromadn\u00E1 operace vr\u00E1cena: " + action });
    }

    void MainWindow::UndoBulkButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        RestoreLastBulkSnapshot();
    }

    void MainWindow::ApplyBulkStatus(hstring const& status)
    {
        auto indices = VisibleRowIndices();
        indices.erase(std::remove_if(indices.begin(), indices.end(), [&](size_t index)
        {
            return m_rows[index].workflowStatus == status;
        }), indices.end());
        if (indices.empty())
        {
            MessageBoxW(GetActiveWindow(), L"Ve zobrazen\u00FDch titulc\u00EDch nen\u00ED co zm\u011Bnit.",
                L"Hromadn\u00E1 zm\u011Bna stavu", MB_OK | MB_ICONINFORMATION);
            return;
        }

        std::wstring message = L"Nastavit stav \u201E" + std::wstring{ status.c_str() } + L"\u201C u " +
            std::to_wstring(indices.size()) + L" zobrazen\u00FDch titulk\u016F?";
        if (MessageBoxW(GetActiveWindow(), message.c_str(), L"Hromadn\u00E1 zm\u011Bna stavu",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        {
            return;
        }
        CaptureBulkSnapshot(indices, hstring{ L"stav " + std::wstring{ status.c_str() } });
        for (auto const index : indices)
            m_rows[index].workflowStatus = status;
        m_workflowStateDirty = true;
        UpdateDirtyFromRows();
        RefreshQaAll();
        LoadCurrentRow();
        ScheduleWorkspaceDraftSave();
        StatusBarText().Text(hstring{ L"Stav zm\u011Bn\u011Bn u " + std::to_wstring(indices.size()) +
            L" titulk\u016F \u00B7 operaci lze vr\u00E1tit" });
    }

    void MainWindow::BulkReadyButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        ApplyBulkStatus(L"P\u0159ipraveno");
    }

    void MainWindow::BulkApprovedButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        ApplyBulkStatus(L"Schv\u00E1leno");
    }

    void MainWindow::SelectFilter(int32_t index)
    {
        if (index < 0 || index > static_cast<int32_t>(agi::winui::SubtitleFilter::approved))
            return;
        FilterComboBox().SelectedIndex(index);
        m_activeFilter = static_cast<agi::winui::SubtitleFilter>(index);
        RefreshProgressSummary();
        RefreshSearchSummary();
        StatusBarText().Text(hstring{ L"Filtr titulk\u016F zm\u011Bn\u011Bn \u00B7 Ctrl+" + std::to_wstring(index + 1) });
    }

    void MainWindow::ConsistencyCheckButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        struct ConsistencyIssue
        {
            size_t index{};
            std::wstring description;
        };
        std::vector<ConsistencyIssue> issues;
        std::map<std::wstring, std::pair<std::wstring, size_t>> knownTranslations;

        for (size_t index = 0; index < m_rows.size(); ++index)
        {
            auto const& row = m_rows[index];
            if (IsTranslationEmpty(row.target))
                continue;
            auto const originalKey = agi::winui::ConsistencyTextKey(row.original.c_str());
            auto const targetKey = agi::winui::ConsistencyTextKey(row.target.c_str());
            if (!originalKey.empty())
            {
                auto const existing = knownTranslations.find(originalKey);
                if (existing == knownTranslations.end())
                    knownTranslations.emplace(originalKey, std::make_pair(targetKey, index));
                else if (existing->second.first != targetKey)
                    issues.push_back({ index, L"stejn\u00FD origin\u00E1l m\u00E1 rozd\u00EDln\u00FD p\u0159eklad" });
            }

            auto const targetView = std::wstring_view{ row.target.c_str(), row.target.size() };
            for (auto const& number : agi::winui::NumberTokens(row.original.c_str()))
            {
                if (targetView.find(number) == std::wstring_view::npos)
                {
                    issues.push_back({ index, L"v p\u0159ekladu chyb\u00ED \u010D\u00EDslo " + number });
                    break;
                }
            }

            auto const sourcePunctuation = agi::winui::TerminalPunctuation(row.original.c_str());
            auto const targetPunctuation = agi::winui::TerminalPunctuation(row.target.c_str());
            if (sourcePunctuation != 0 && sourcePunctuation != targetPunctuation)
            {
                issues.push_back({ index, L"odli\u0161n\u00E1 koncov\u00E1 interpunkce" });
            }
        }

        if (issues.empty())
        {
            MessageBoxW(GetActiveWindow(),
                L"Nebyl nalezen rozd\u00EDln\u00FD p\u0159eklad stejn\u00E9ho textu ani podez\u0159el\u00E9 rozd\u00EDly v \u010D\u00EDslech a interpunkci.",
                L"Kontrola konzistence", MB_OK | MB_ICONINFORMATION);
            return;
        }

        std::wstring message = L"Nalezeno " + std::to_wstring(issues.size()) + L" upozorn\u011Bn\u00ED:";
        for (size_t issueIndex = 0; issueIndex < (std::min)(issues.size(), size_t{ 15 }); ++issueIndex)
        {
            auto const& issue = issues[issueIndex];
            message += L"\n#" + std::to_wstring(m_rows[issue.index].number) + L" \u00B7 " + issue.description;
        }
        if (issues.size() > 15)
            message += L"\n\u2026 a dal\u0161\u00EDch " + std::to_wstring(issues.size() - 15);
        message += L"\n\nEditor p\u0159ejde na prvn\u00ED upozorn\u011Bn\u00ED.";
        MessageBoxW(GetActiveWindow(), message.c_str(), L"Kontrola konzistence", MB_OK | MB_ICONWARNING);
        StoreCurrentEditorSelection();
        m_currentIndex = static_cast<int32_t>(issues.front().index);
        LoadCurrentRow();
        TargetTextBox().Focus(FocusState::Programmatic);
        StatusBarText().Text(hstring{ L"Kontrola konzistence \u00B7 " + std::to_wstring(issues.size()) + L" upozorn\u011Bn\u00ED" });
    }

    void MainWindow::RefreshProjectOverview()
    {
        auto const total = m_rows.size();
        auto const untranslated = static_cast<size_t>(std::count_if(m_rows.begin(), m_rows.end(), [this](auto const& row)
        {
            return IsTranslationEmpty(row.target);
        }));
        auto const problems = static_cast<size_t>(std::count_if(m_rows.begin(), m_rows.end(), [](auto const& row)
        {
            return !row.qaIssue.empty();
        }));
        auto const approved = static_cast<size_t>(std::count_if(m_rows.begin(), m_rows.end(), [](auto const& row)
        {
            return row.workflowStatus == L"Schv\u00E1leno" && row.qaIssue.empty();
        }));
        auto const translated = total - untranslated;
        auto const visible = VisibleRowIndices().size();
        double const percentage = total == 0 ? 0.0 : 100.0 * static_cast<double>(translated) / total;
        OverviewProgressBar().Value(percentage);
        OverviewTranslatedText().Text(hstring{ L"P\u0159elo\u017Eeno: " + std::to_wstring(translated) + L"/" + std::to_wstring(total) });
        OverviewApprovedText().Text(hstring{ L"Schv\u00E1leno: " + std::to_wstring(approved) });
        OverviewProblemsText().Text(hstring{ L"Probl\u00E9my QA: " + std::to_wstring(problems) });
        OverviewVisibleText().Text(hstring{ L"Aktu\u00E1ln\u00ED filtr: " + std::to_wstring(visible) + L" zobrazeno" });
        auto const remaining = total - approved;
        auto const estimateMinutes = (remaining * 45 + 59) / 60;
        OverviewEstimateText().Text(hstring{ L"Zb\u00FDv\u00E1 ke kontrole: " + std::to_wstring(remaining) +
            L" \u00B7 orienta\u010Dn\u011B " + std::to_wstring(estimateMinutes) + L" min" });

        std::wstring files = m_sourcePath.empty() ? L"Uk\u00E1zkov\u00E1 data" :
            std::filesystem::path(m_sourcePath.c_str()).filename().wstring();
        if (!m_targetPath.empty())
            files += L" \u2192 " + std::filesystem::path(m_targetPath.c_str()).filename().wstring();
        OverviewFilesText().Text(hstring{ files });
    }

    void MainWindow::TargetTextBox_TextChanged(
        Windows::Foundation::IInspectable const&,
        TextChangedEventArgs const&)
    {
        if (m_loadingSelection || !m_initialized || m_rows.empty())
        {
            return;
        }

        ClearBulkUndo();

        auto& row = m_rows[m_currentIndex];
        auto const newText = TargetTextBox().Text();
        bool const applyingHistory = m_hasPendingHistoryText &&
            agi::winui::EquivalentEditorText(newText.c_str(), m_pendingHistoryText.c_str());
        if (applyingHistory)
        {
            m_hasPendingHistoryText = false;
            m_pendingHistoryText = L"";
        }
        if (agi::winui::EquivalentEditorText(newText.c_str(), row.target.c_str()))
        {
            if (applyingHistory)
                row.editSequenceKind = 0;
            return;
        }
        if (!row.historyInitialized)
        {
            row.savedTarget = row.target;
            row.historyInitialized = true;
        }
        if (!applyingHistory && newText != row.target)
        {
            std::wstring const before{ row.target.c_str() };
            std::wstring const after{ newText.c_str() };
            auto const edit = ClassifySingleCharacterEdit(before, after);
            bool const continuesInsertion = edit.kind == 1
                && row.editSequenceKind == 1
                && edit.position == row.editSequencePosition;
            bool const continuesDeletion = edit.kind == 2
                && row.editSequenceKind == 2
                && (edit.position == row.editSequencePosition
                    || edit.position + 1 == row.editSequencePosition);

            if (!continuesInsertion && !continuesDeletion)
                CaptureWorkspaceUndoSnapshot(L"úprava textu");
            row.editSequenceKind = edit.kind;
            row.editSequencePosition = edit.position + (edit.kind == 1 ? 1 : 0);
        }
        else if (applyingHistory)
        {
            row.editSequenceKind = 0;
        }

        row.target = newText;
        row.targetModified = !agi::winui::EquivalentEditorText(row.target.c_str(), row.savedTarget.c_str());
        row.status = (row.targetModified || row.timingModified)
            ? hstring{ L"Upraveno" }
            : (row.savedWorkflowStatus.empty() ? hstring{ L"Ulo\u017Eeno" } : row.savedWorkflowStatus);
        UpdateDirtyFromRows();
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size()))
        {
            m_targetEntries[m_currentIndex].text = row.target;
        }

        TargetInfoText().Text(hstring{ L"#" + std::to_wstring(row.number) + L" \u00B7 " +
            std::wstring(row.status.c_str()) });
        TargetStatusText().Text(hstring{ L"Stav: " + std::wstring(row.status.c_str()) });
        UpdateTableRow(m_currentIndex);
        UpdateMetrics();
        StatusBarText().Text(row.targetModified
            ? L"Neulo\u017Een\u00E1 zm\u011Bna v aktu\u00E1ln\u00EDm titulku"
            : L"Text odpov\u00EDd\u00E1 ulo\u017Een\u00E9 verzi");
    }

    void MainWindow::SubtitleRow_Tapped(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&)
    {
        auto const index = RowIndexFromSender(sender);
        if (index < 0 || index >= static_cast<int32_t>(m_rows.size()) || index == m_currentIndex)
        {
            return;
        }

        StoreCurrentEditorSelection();
        m_currentIndex = index;
        LoadCurrentRow();
    }

    void MainWindow::LoadCurrentRow()
    {
        if (m_rows.empty())
        {
            return;
        }

        auto& row = m_rows[m_currentIndex];
        row.editSequenceKind = 0;

        if (!m_targetPath.empty())
        {
            row.qaIssue = EvaluateQaIssue(m_currentIndex);
            row.status = row.qaIssue.empty()
                ? (row.workflowStatus.empty() ? winrt::hstring{ L"Připraveno" } : row.workflowStatus)
                : winrt::hstring{ L"Problém" };
            UpdateTableRow(m_currentIndex);
        }

        m_loadingSelection = true;

        std::wstring header = L"Aktu\u00E1ln\u00ED titulek #" + std::to_wstring(row.number);
        header += L" \u00B7 ";
        header += row.start.c_str();
        header += L" \u2192 ";
        header += row.end.c_str();
        header += L" \u00B7 d\u00E9lka ";

        std::wostringstream durationStream;
        durationStream << std::fixed << std::setprecision(2) << row.duration;
        header += durationStream.str();
        header += L" s";
        HeaderCurrentSubtitleText().Text(hstring{ header });

        if (row.sourceStart.empty())
        {
            OriginalTimingText().Text(hstring{ L"#" + std::to_wstring(row.number) + L" \u00B7 bez \u010Dasov\u00E9ho p\u00E1ru" });
        }
        else
        {
            std::wstring timing = L"#" + std::to_wstring(row.number);
            timing += L" \u00B7 ";
            timing += row.sourceStart.c_str();
            timing += L" \u2192 ";
            timing += row.sourceEnd.c_str();
            OriginalTimingText().Text(hstring{ timing });
        }
        OriginalTextBox().Text(row.original);
        RefreshGlossaryForCurrentSubtitle();

        std::wstring targetInfo = L"#" + std::to_wstring(row.number) + L" \u00B7 ";
        targetInfo += row.status.c_str();
        TargetInfoText().Text(hstring{ targetInfo });
        RefreshCurrentProblemText();
        RefreshTimingEditor();
        TargetTextBox().Text(row.target);
        if (row.selectionInitialized)
        {
            auto const textLength = static_cast<int32_t>(row.target.size());
            auto const selectionStart = (std::max)(0, (std::min)(textLength, row.selectionStart));
            auto const selectionLength = (std::max)(0,
                (std::min)(textLength - selectionStart, row.selectionLength));
            TargetTextBox().SelectionStart(selectionStart);
            TargetTextBox().SelectionLength(selectionLength);
        }
        else
        {
            TargetTextBox().SelectionStart(0);
            TargetTextBox().SelectionLength(0);
        }

        std::wstring status = L"Stav: ";
        status += row.status.c_str();
        TargetStatusText().Text(hstring{ status });
        RefreshApprovalAction();

        RefreshTranscriptContext();

        bool const subtitleSelectionChanged = m_waveformViewportSubtitleIndex != m_currentIndex;
        if (subtitleSelectionChanged)
        {
            m_waveformViewportSubtitleIndex = m_currentIndex;
            CenterWaveformOnCurrentSubtitle();
            if (!m_mediaDrivenSelectionUpdate)
                SeekVideoToCurrentSubtitle();
        }
        RenderWaveform();
        RefreshWaveformPlayhead();

        std::wstring tablePosition =
            L"#" + std::to_wstring(row.number) + L" / " + std::to_wstring(m_rows.size()) +
            L" · " + std::wstring{ row.start.c_str() } + L" → " + std::wstring{ row.end.c_str() };
        auto const videoSeconds = CurrentVideoSeconds();
        if (videoSeconds >= 0.0)
            tablePosition += L" · video " + std::wstring{ FormatWinUiTiming(videoSeconds).c_str() };
        TablePositionText().Text(hstring{ tablePosition });

        UpdateSelectionVisuals();
        ScrollCurrentRowIntoView();
        UpdateMetrics();

        if (m_sourcePath.empty())
        {
            if (m_targetPath.empty())
                StatusBarText().Text(L"Samostatn\u00FD projekt titulk\u016F");
            else
            {
                auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
                StatusBarText().Text(hstring{ L"P\u0159eklad: " + targetName });
            }
        }
        else if (m_targetPath.empty())
        {
            auto const sourceName = std::filesystem::path(m_sourcePath.c_str()).filename().wstring();
            StatusBarText().Text(hstring{
                L"Origin\u00E1l: " + sourceName + L" \u00B7 p\u0159eklad nen\u00ED na\u010Dten" });
        }
        else
        {
            auto const sourceName = std::filesystem::path(m_sourcePath.c_str()).filename().wstring();
            auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
            StatusBarText().Text(hstring{
                L"Origin\u00E1l: " + sourceName + L" \u00B7 P\u0159eklad: " + targetName +
                L" \u00B7 p\u00E1rov\u00E1n\u00ED podle \u010Dasu" });
        }

        m_loadingSelection = false;
    }

    void MainWindow::RefreshTranscriptContext()
    {
        auto hideContextBlocks = [this]()
        {
            TranscriptPrev3Block().Visibility(Visibility::Collapsed);
            TranscriptPrev2Block().Visibility(Visibility::Collapsed);
            TranscriptPreviousBlock().Visibility(Visibility::Collapsed);
            TranscriptNextBlock().Visibility(Visibility::Collapsed);
            TranscriptNext2Block().Visibility(Visibility::Collapsed);
            TranscriptNext3Block().Visibility(Visibility::Collapsed);
        };

        if (m_rows.empty() || m_currentIndex < 0 ||
            m_currentIndex >= static_cast<int32_t>(m_rows.size()))
        {
            hideContextBlocks();
            TranscriptCurrentTimeText().Text(L"");
            TranscriptCurrentText().Text(L"");
            return;
        }

        auto const& row = m_rows[m_currentIndex];

        if (!m_transcriptChunks.empty())
        {
            std::wstring reference = row.original.empty()
                ? std::wstring{ row.target.c_str() }
                : std::wstring{ row.original.c_str() };

            int32_t bestIndex = -1;
            double bestScore = 0.0;
            for (int32_t index = 0;
                index < static_cast<int32_t>(m_transcriptChunks.size()); ++index)
            {
                auto const score = TranscriptMatchScore(
                    reference, std::wstring_view{ m_transcriptChunks[index].c_str() });
                if (score > bestScore)
                {
                    bestScore = score;
                    bestIndex = index;
                }
            }

            if (bestIndex < 0 || bestScore < 0.08)
            {
                if (m_transcriptChunks.size() <= 1 || m_rows.size() <= 1)
                {
                    bestIndex = 0;
                }
                else
                {
                    auto const ratio = static_cast<double>(m_currentIndex) /
                        static_cast<double>(m_rows.size() - 1);
                    bestIndex = static_cast<int32_t>(std::llround(
                        ratio * static_cast<double>(m_transcriptChunks.size() - 1)));
                }
            }

            auto setTranscriptChunk = [this](
                int32_t index,
                winrt::Microsoft::UI::Xaml::Controls::StackPanel const& block,
                winrt::Microsoft::UI::Xaml::Controls::TextBlock const& labelText,
                winrt::Microsoft::UI::Xaml::Controls::TextBlock const& bodyText)
            {
                if (index < 0 || index >= static_cast<int32_t>(m_transcriptChunks.size()))
                {
                    block.Visibility(Visibility::Collapsed);
                    return;
                }

                block.Visibility(Visibility::Visible);
                labelText.Text(winrt::hstring{
                    L"část " + std::to_wstring(index + 1) +
                    L" / " + std::to_wstring(m_transcriptChunks.size()) });
                bodyText.Text(m_transcriptChunks[index]);
            };

            setTranscriptChunk(bestIndex - 3, TranscriptPrev3Block(), TranscriptPrev3TimeText(), TranscriptPrev3Text());
            setTranscriptChunk(bestIndex - 2, TranscriptPrev2Block(), TranscriptPrev2TimeText(), TranscriptPrev2Text());
            setTranscriptChunk(bestIndex - 1, TranscriptPreviousBlock(), TranscriptPreviousTimeText(), TranscriptPreviousText());

            TranscriptCurrentTimeText().Text(winrt::hstring{
                L"část " + std::to_wstring(bestIndex + 1) +
                L" / " + std::to_wstring(m_transcriptChunks.size()) });
            TranscriptCurrentText().Text(m_transcriptChunks[bestIndex]);

            setTranscriptChunk(bestIndex + 1, TranscriptNextBlock(), TranscriptNextTimeText(), TranscriptNextText());
            setTranscriptChunk(bestIndex + 2, TranscriptNext2Block(), TranscriptNext2TimeText(), TranscriptNext2Text());
            setTranscriptChunk(bestIndex + 3, TranscriptNext3Block(), TranscriptNext3TimeText(), TranscriptNext3Text());
            return;
        }

        if (!m_transcriptEntries.empty())
        {
            auto const rowStart = WorkflowTimestampSeconds(row.start);
            auto const rowEnd = WorkflowTimestampSeconds(row.end);
            auto const rowCenter = (rowStart + rowEnd) * 0.5;

            int32_t bestIndex = 0;
            double bestQuality = -1.0;
            double bestCenterDistance = (std::numeric_limits<double>::max)();

            for (int32_t index = 0;
                index < static_cast<int32_t>(m_transcriptEntries.size()); ++index)
            {
                auto const& entry = m_transcriptEntries[index];
                auto const quality = agi::winui::SubtitleOverlapQuality(
                    rowStart, rowEnd, entry.startSeconds, entry.endSeconds);
                auto const entryCenter = (entry.startSeconds + entry.endSeconds) * 0.5;
                auto const centerDistance = std::abs(rowCenter - entryCenter);

                if (quality > bestQuality ||
                    (std::abs(quality - bestQuality) < 0.000001 &&
                        centerDistance < bestCenterDistance))
                {
                    bestQuality = quality;
                    bestCenterDistance = centerDistance;
                    bestIndex = index;
                }
            }

            auto setTranscriptEntry = [this](
                int32_t index,
                winrt::Microsoft::UI::Xaml::Controls::StackPanel const& block,
                winrt::Microsoft::UI::Xaml::Controls::TextBlock const& timeText,
                winrt::Microsoft::UI::Xaml::Controls::TextBlock const& bodyText)
            {
                if (index < 0 || index >= static_cast<int32_t>(m_transcriptEntries.size()))
                {
                    block.Visibility(Visibility::Collapsed);
                    return;
                }

                auto const& entry = m_transcriptEntries[index];
                block.Visibility(Visibility::Visible);
                timeText.Text(entry.start);
                bodyText.Text(entry.text);
            };

            setTranscriptEntry(bestIndex - 3, TranscriptPrev3Block(), TranscriptPrev3TimeText(), TranscriptPrev3Text());
            setTranscriptEntry(bestIndex - 2, TranscriptPrev2Block(), TranscriptPrev2TimeText(), TranscriptPrev2Text());
            setTranscriptEntry(bestIndex - 1, TranscriptPreviousBlock(), TranscriptPreviousTimeText(), TranscriptPreviousText());

            auto const& current = m_transcriptEntries[bestIndex];
            TranscriptCurrentTimeText().Text(current.start);
            TranscriptCurrentText().Text(current.text);

            setTranscriptEntry(bestIndex + 1, TranscriptNextBlock(), TranscriptNextTimeText(), TranscriptNextText());
            setTranscriptEntry(bestIndex + 2, TranscriptNext2Block(), TranscriptNext2TimeText(), TranscriptNext2Text());
            setTranscriptEntry(bestIndex + 3, TranscriptNext3Block(), TranscriptNext3TimeText(), TranscriptNext3Text());
            return;
        }

        auto setOriginalContext = [this](
            int32_t index,
            winrt::Microsoft::UI::Xaml::Controls::StackPanel const& block,
            winrt::Microsoft::UI::Xaml::Controls::TextBlock const& timeText,
            winrt::Microsoft::UI::Xaml::Controls::TextBlock const& bodyText)
        {
            if (index < 0 || index >= static_cast<int32_t>(m_rows.size()))
            {
                block.Visibility(Visibility::Collapsed);
                return;
            }

            auto const& contextRow = m_rows[index];
            block.Visibility(Visibility::Visible);
            timeText.Text(contextRow.start);
            bodyText.Text(contextRow.original);
        };

        setOriginalContext(m_currentIndex - 3, TranscriptPrev3Block(), TranscriptPrev3TimeText(), TranscriptPrev3Text());
        setOriginalContext(m_currentIndex - 2, TranscriptPrev2Block(), TranscriptPrev2TimeText(), TranscriptPrev2Text());
        setOriginalContext(m_currentIndex - 1, TranscriptPreviousBlock(), TranscriptPreviousTimeText(), TranscriptPreviousText());

        TranscriptCurrentTimeText().Text(row.start);
        TranscriptCurrentText().Text(row.original.empty()
            ? hstring{ L"Na\u010Dt\u011Bte origin\u00E1l nebo samostatn\u00FD transcript." }
            : row.original);

        setOriginalContext(m_currentIndex + 1, TranscriptNextBlock(), TranscriptNextTimeText(), TranscriptNextText());
        setOriginalContext(m_currentIndex + 2, TranscriptNext2Block(), TranscriptNext2TimeText(), TranscriptNext2Text());
        setOriginalContext(m_currentIndex + 3, TranscriptNext3Block(), TranscriptNext3TimeText(), TranscriptNext3Text());
    }

    void MainWindow::RefreshCurrentProblemText()
    {
        if (m_rows.empty() || m_currentIndex < 0 ||
            m_currentIndex >= static_cast<int32_t>(m_rows.size()))
        {
            TargetProblemText().Text(L"");
            TargetProblemText().Visibility(Visibility::Collapsed);
            ToolTipService::SetToolTip(TargetProblemText(), nullptr);
            return;
        }

        auto const& row = m_rows[m_currentIndex];
        if (row.qaIssue.empty())
        {
            TargetProblemText().Text(L"");
            TargetProblemText().Visibility(Visibility::Collapsed);
            ToolTipService::SetToolTip(TargetProblemText(), nullptr);
            return;
        }

        auto const detail = winrt::hstring{
            std::wstring{ L"⚠ " } + row.qaIssue.c_str() };
        TargetProblemText().Text(detail);
        TargetProblemText().Visibility(Visibility::Visible);
        ToolTipService::SetToolTip(TargetProblemText(), winrt::box_value(row.qaIssue));
    }

    void MainWindow::UpdateMetrics()
    {
        if (m_rows.empty())
        {
            TargetCplText().Text(L"CPL 0");
            TargetCpsText().Text(L"CPS 0.0");
            return;
        }

        auto const& row = m_rows[m_currentIndex];
        std::wstring const text{ row.target.c_str() };

        size_t currentLineLength = 0;
        size_t maxLineLength = 0;
        size_t characterCount = 0;

        for (size_t index = 0; index < text.size(); ++index)
        {
            auto const character = text[index];
            if (character == L'\r' || character == L'\n')
            {
                if (character == L'\r' && index + 1 < text.size() && text[index + 1] == L'\n')
                    ++index;
                maxLineLength = (std::max)(maxLineLength, currentLineLength);
                currentLineLength = 0;
                continue;
            }

            ++currentLineLength;
            ++characterCount;
        }

        maxLineLength = (std::max)(maxLineLength, currentLineLength);
        double const cps = row.duration > 0.0
            ? static_cast<double>(characterCount) / row.duration
            : 0.0;

        TargetCplText().Text(hstring{ L"CPL " + std::to_wstring(maxLineLength) });

        std::wostringstream cpsStream;
        cpsStream << L"CPS " << std::fixed << std::setprecision(1) << cps;
        TargetCpsText().Text(hstring{ cpsStream.str() });
    }

    bool MainWindow::IsSubtitleRowSelected(int32_t index) const
    {
        return std::find(m_selectedSubtitleIndices.begin(), m_selectedSubtitleIndices.end(), index)
            != m_selectedSubtitleIndices.end();
    }

    void MainWindow::NormalizeSubtitleSelection()
    {
        m_selectedSubtitleIndices.erase(
            std::remove_if(
                m_selectedSubtitleIndices.begin(),
                m_selectedSubtitleIndices.end(),
                [this](int32_t index)
                {
                    return index < 0 || index >= static_cast<int32_t>(m_rows.size());
                }),
            m_selectedSubtitleIndices.end());

        std::sort(m_selectedSubtitleIndices.begin(), m_selectedSubtitleIndices.end());
        m_selectedSubtitleIndices.erase(
            std::unique(m_selectedSubtitleIndices.begin(), m_selectedSubtitleIndices.end()),
            m_selectedSubtitleIndices.end());

        if (m_selectedSubtitleIndices.empty() && !m_rows.empty())
            m_selectedSubtitleIndices.push_back(m_currentIndex);
    }

    void MainWindow::RefreshSubtitleSelectionText()
    {
        NormalizeSubtitleSelection();
        auto const count = m_selectedSubtitleIndices.size();
        if (count == 0)
        {
            SubtitleSelectionText().Text(L"0 titulků");
            return;
        }
        if (count == 1)
        {
            SubtitleSelectionText().Text(L"Vybrán 1 titulek");
            return;
        }

        SubtitleSelectionText().Text(winrt::hstring{
            L"Vybráno " + std::to_wstring(count) + L" titulků" });
    }

    void MainWindow::SelectSubtitleRow(int32_t index, bool ctrl, bool shift)
    {
        if (index < 0 || index >= static_cast<int32_t>(m_rows.size()))
            return;

        if (shift && m_selectionAnchorIndex >= 0)
        {
            auto const first = (std::min)(m_selectionAnchorIndex, index);
            auto const last = (std::max)(m_selectionAnchorIndex, index);
            m_selectedSubtitleIndices.clear();
            for (int32_t row = first; row <= last; ++row)
                m_selectedSubtitleIndices.push_back(row);
        }
        else if (ctrl)
        {
            auto const it = std::find(
                m_selectedSubtitleIndices.begin(),
                m_selectedSubtitleIndices.end(),
                index);
            if (it == m_selectedSubtitleIndices.end())
                m_selectedSubtitleIndices.push_back(index);
            else if (m_selectedSubtitleIndices.size() > 1)
                m_selectedSubtitleIndices.erase(it);

            m_selectionAnchorIndex = index;
        }
        else
        {
            m_selectedSubtitleIndices.assign(1, index);
            m_selectionAnchorIndex = index;
        }

        NormalizeSubtitleSelection();
        UpdateSelectionVisuals();
        RefreshSubtitleSelectionText();
    }

    void MainWindow::UpdateSelectionVisuals()
    {
        auto const accentBrush = TargetPanelBorder().BorderBrush();

        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush transparentBrush;
        transparentBrush.Color(winrt::Windows::UI::Color{ 0, 0, 0, 0 });

        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush selectedBrush;
        selectedBrush.Color(winrt::Windows::UI::Color{ 34, 0, 120, 212 });

        Microsoft::UI::Xaml::Media::SolidColorBrush separatorBrush;
        separatorBrush.Color(Windows::UI::Color{ 42, 128, 128, 128 });

        for (int32_t index = 0; index < static_cast<int32_t>(m_rowBorders.size()); ++index)
        {
            auto const& border = m_rowBorders[index];
            border.BorderBrush(separatorBrush);
            border.BorderThickness(Thickness{ 0.0, 0.0, 0.0, 1.0 });
            border.Background(IsSubtitleRowSelected(index) ? selectedBrush : transparentBrush);
        }

        if (m_currentIndex >= 0 && m_currentIndex < static_cast<int32_t>(m_rowBorders.size()))
        {
            m_rowBorders[m_currentIndex].BorderBrush(accentBrush);
            m_rowBorders[m_currentIndex].BorderThickness(Thickness{ 4.0, 0.0, 0.0, 1.0 });
        }

        RefreshSubtitleSelectionText();
    }

    void MainWindow::StoreCurrentEditorSelection()
    {
        if (m_loadingSelection || m_rows.empty() || m_currentIndex < 0
            || m_currentIndex >= static_cast<int32_t>(m_rows.size()))
        {
            return;
        }

        auto& row = m_rows[m_currentIndex];
        row.selectionStart = TargetTextBox().SelectionStart();
        row.selectionLength = TargetTextBox().SelectionLength();
        row.selectionInitialized = true;
        row.editSequenceKind = 0;
    }

    void MainWindow::ScrollCurrentRowIntoView()
    {
        if (m_currentIndex < 0 || m_currentIndex >= static_cast<int32_t>(m_rowBorders.size()))
        {
            return;
        }

        // First make sure the selected row itself is visible.
        m_rowBorders[m_currentIndex].StartBringIntoView();

        // Then bring the second following visible row into view as well. Because it is
        // only two rows below the selection, a normal subtitle-list viewport keeps all
        // three rows visible and the active subtitle is no longer pinned to the bottom.
        int32_t lookAheadIndex = m_currentIndex;
        int visibleRowsAfter = 0;

        for (int32_t index = m_currentIndex + 1;
            index < static_cast<int32_t>(m_rows.size()) &&
            index < static_cast<int32_t>(m_rowBorders.size());
            ++index)
        {
            if (!RowMatchesActiveFilter(m_rows[index]))
                continue;

            lookAheadIndex = index;
            if (++visibleRowsAfter >= 2)
                break;
        }

        if (lookAheadIndex != m_currentIndex)
            m_rowBorders[lookAheadIndex].StartBringIntoView();
    }

    void MainWindow::UpdateTableRow(int32_t index)
    {
        if (index < 0 || index >= static_cast<int32_t>(m_rows.size()) ||
            index >= static_cast<int32_t>(m_rowTargetTexts.size()) ||
            index >= static_cast<int32_t>(m_rowStatusTexts.size()))
        {
            return;
        }

        m_rowTargetTexts[index].Text(m_rows[index].target);
        m_rowStatusTexts[index].Text(m_rows[index].status);
        if (!m_rows[index].qaIssue.empty())
            ToolTipService::SetToolTip(m_rowStatusTexts[index], winrt::box_value(m_rows[index].qaIssue));
        else
            ToolTipService::SetToolTip(m_rowStatusTexts[index], nullptr);
        RefreshSearchHighlights();
    }

    int32_t MainWindow::RowIndexFromSender(
        Windows::Foundation::IInspectable const& sender) const
    {
        auto const element = sender.try_as<FrameworkElement>();
        if (!element)
        {
            return -1;
        }

        auto const name = element.Name();
        if (name == L"Row143Border") return 0;
        if (name == L"Row144Border") return 1;
        if (name == L"Row145Border") return 2;
        if (name == L"Row146Border") return 3;
        if (name == L"Row147Border") return 4;

        return -1;
    }

    void MainWindow::InitializeDynamicSubtitleGrid()
    {
        m_subtitleGrid = SubtitleGridHost();
    }

    void MainWindow::RebuildSubtitleGrid()
    {
        if (!m_subtitleGrid)
        {
            return;
        }

        auto const grid = m_subtitleGrid;
        grid.Children().Clear();
        grid.RowDefinitions().Clear();

        m_rowBorders.clear();
        m_rowOriginalTexts.clear();
        m_rowTargetTexts.clear();
        m_rowStatusTexts.clear();
        m_rowVisuals.clear();
        m_rowVisuals.resize(m_rows.size());

        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            RowDefinition rowDefinition;
            rowDefinition.Height(GridLength{ 31.0, GridUnitType::Pixel });
            grid.RowDefinitions().Append(rowDefinition);
        }

        auto addText = [&](hstring const& text, int32_t row, int32_t column, bool ellipsis, double leftMargin)
        {
            TextBlock block;
            block.Text(text);
            block.Margin(Thickness{ leftMargin, 0.0, 4.0, 0.0 });
            block.VerticalAlignment(VerticalAlignment::Center);
            block.IsHitTestVisible(false);
            block.FontSize(11.5);
            if (ellipsis)
                block.TextTrimming(TextTrimming::CharacterEllipsis);
            Grid::SetRow(block, row);
            Grid::SetColumn(block, column);
            grid.Children().Append(block);
            return block;
        };

        for (int32_t index = 0; index < static_cast<int32_t>(m_rows.size()); ++index)
        {
            auto const& row = m_rows[index];
            auto const visualRow = index;

            Microsoft::UI::Xaml::Media::SolidColorBrush transparentBrush;
            transparentBrush.Color(Windows::UI::Color{ 0, 0, 0, 0 });

            Border rowBorder;
            rowBorder.Background(transparentBrush);

            Microsoft::UI::Xaml::Media::SolidColorBrush separatorBrush;
            separatorBrush.Color(Windows::UI::Color{ 42, 128, 128, 128 });
            rowBorder.BorderBrush(separatorBrush);
            rowBorder.BorderThickness(Thickness{ 0.0, 0.0, 0.0, 1.0 });

            Grid::SetRow(rowBorder, visualRow);
            Grid::SetColumnSpan(rowBorder, 6);
            rowBorder.PointerPressed([this, index](
                auto const& sender,
                winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
            {
                auto const border = sender.as<winrt::Microsoft::UI::Xaml::Controls::Border>();
                auto const point = args.GetCurrentPoint(border);
                if (!point.Properties().IsLeftButtonPressed())
                    return;

                m_subtitleDragSelecting = true;
                m_subtitleDragSelectionMoved = false;
                m_subtitleDragAnchor = index;
            });

            rowBorder.PointerEntered([this, index](
                auto const& sender,
                winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
            {
                if (!m_subtitleDragSelecting || m_subtitleDragAnchor < 0)
                    return;

                auto const border = sender.as<winrt::Microsoft::UI::Xaml::Controls::Border>();
                auto const point = args.GetCurrentPoint(border);
                if (!point.Properties().IsLeftButtonPressed() || index == m_subtitleDragAnchor)
                    return;

                m_subtitleDragSelectionMoved = true;
                auto const first = (std::min)(m_subtitleDragAnchor, index);
                auto const last = (std::max)(m_subtitleDragAnchor, index);

                m_selectedSubtitleIndices.clear();
                for (int32_t rowIndex = first; rowIndex <= last; ++rowIndex)
                    m_selectedSubtitleIndices.push_back(rowIndex);

                UpdateSelectionVisuals();
                RefreshSubtitleSelectionText();
            });

            rowBorder.PointerReleased([this, index](
                auto const&,
                winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
            {
                if (!m_subtitleDragSelecting)
                    return;

                m_subtitleDragSelecting = false;
                if (!m_subtitleDragSelectionMoved)
                    return;

                if (index != m_currentIndex)
                    StoreCurrentEditorSelection();

                m_currentIndex = index;
                m_selectionAnchorIndex = m_subtitleDragAnchor;
                LoadCurrentRow();
                TargetTextBox().Focus(FocusState::Programmatic);
                args.Handled(true);
            });

            rowBorder.Tapped([this, index](auto const&, auto const&)
            {
                if (m_subtitleDragSelectionMoved)
                {
                    m_subtitleDragSelectionMoved = false;
                    return;
                }

                if (index < 0 || index >= static_cast<int32_t>(m_rows.size()))
                    return;

                bool const ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool const shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

                if (index != m_currentIndex)
                    StoreCurrentEditorSelection();

                SelectSubtitleRow(index, ctrl, shift);
                m_currentIndex = index;
                LoadCurrentRow();
                TargetTextBox().Focus(FocusState::Programmatic);
            });
            MenuFlyout rowMenu;

            auto activateContextRow = [this, index]()
            {
                if (index < 0 || index >= static_cast<int32_t>(m_rows.size()))
                    return false;

                if (index != m_currentIndex || !IsSubtitleRowSelected(index))
                {
                    if (index != m_currentIndex)
                        StoreCurrentEditorSelection();
                    SelectSubtitleRow(index, false, false);
                    m_currentIndex = index;
                    LoadCurrentRow();
                }
                return true;
            };

            MenuFlyoutItem insertAboveItem;
            insertAboveItem.Text(L"Přidat titulek nad");
            insertAboveItem.Click([this, activateContextRow](auto const&, auto const&)
            {
                if (activateContextRow())
                    InsertSubtitleRelative(true);
            });
            rowMenu.Items().Append(insertAboveItem);

            MenuFlyoutItem insertBelowItem;
            insertBelowItem.Text(L"Přidat titulek pod");
            insertBelowItem.Click([this, activateContextRow](auto const&, auto const&)
            {
                if (activateContextRow())
                    InsertSubtitleRelative(false);
            });
            rowMenu.Items().Append(insertBelowItem);

            MenuFlyoutItem insertAtVideoItem;
            insertAtVideoItem.Text(L"Přidat titulek na pozici videa");
            insertAtVideoItem.Click([this](auto const&, auto const&)
            {
                InsertSubtitleAtVideoPosition();
            });
            rowMenu.Items().Append(insertAtVideoItem);

            MenuFlyoutItem duplicateItem;
            duplicateItem.Text(L"Zdvojit řádek");
            duplicateItem.Click([this, activateContextRow](auto const&, auto const&)
            {
                if (activateContextRow())
                    DuplicateCurrentSubtitle();
            });
            rowMenu.Items().Append(duplicateItem);

            rowMenu.Items().Append(MenuFlyoutSeparator{});

            MenuFlyoutItem mergeItem;
            mergeItem.Text(L"Sloučit vybrané titulky");
            mergeItem.Click([this, index](auto const&, auto const&)
            {
                if (!IsSubtitleRowSelected(index))
                {
                    SelectSubtitleRow(index, false, false);
                    m_currentIndex = index;
                    LoadCurrentRow();
                }
                MergeSelectedSubtitles();
            });
            rowMenu.Items().Append(mergeItem);

            rowMenu.Items().Append(MenuFlyoutSeparator{});

            MenuFlyoutItem deleteItem;
            deleteItem.Text(L"Smazat titulek");
            deleteItem.Click([this, activateContextRow](auto const&, auto const&)
            {
                if (activateContextRow())
                    DeleteCurrentSubtitle();
            });
            rowMenu.Items().Append(deleteItem);

            rowBorder.ContextFlyout(rowMenu);

            grid.Children().Append(rowBorder);
            m_rowBorders.push_back(rowBorder);
            auto& visuals = m_rowVisuals[index];
            visuals.push_back(rowBorder.as<UIElement>());

            auto const numberText = addText(hstring{ std::to_wstring(row.number) }, visualRow, 0, false, 10.0);
            auto const startText = addText(row.start, visualRow, 1, false, 8.0);
            auto const endText = addText(row.end, visualRow, 2, false, 8.0);
            auto const originalText = addText(row.original, visualRow, 3, true, 8.0);
            auto const targetText = addText(row.target, visualRow, 4, true, 8.0);
            auto const statusText = addText(row.status, visualRow, 5, true, 8.0);
            statusText.IsHitTestVisible(true);
            if (!row.qaIssue.empty())
                ToolTipService::SetToolTip(statusText, winrt::box_value(row.qaIssue));
            else
                ToolTipService::SetToolTip(statusText, nullptr);

            visuals.push_back(numberText.as<UIElement>());
            visuals.push_back(startText.as<UIElement>());
            visuals.push_back(endText.as<UIElement>());
            visuals.push_back(originalText.as<UIElement>());
            visuals.push_back(targetText.as<UIElement>());
            visuals.push_back(statusText.as<UIElement>());
            m_rowOriginalTexts.push_back(originalText);
            m_rowTargetTexts.push_back(targetText);
            m_rowStatusTexts.push_back(statusText);
        }

        RefreshActiveFilter();
        UpdateSelectionVisuals();
        RefreshSearchHighlights();
    }

    void MainWindow::TargetSelectAllMenuItem_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        TargetTextBox().SelectAll();
        TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    void MainWindow::TargetCopyMenuItem_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const text = std::wstring{ TargetTextBox().Text().c_str() };
        auto const start = static_cast<size_t>((std::max)(0, TargetTextBox().SelectionStart()));
        auto const length = static_cast<size_t>((std::max)(0, TargetTextBox().SelectionLength()));
        if (length == 0 || start >= text.size())
            return;

        auto const safeLength = (std::min)(length, text.size() - start);
        winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
        package.SetText(winrt::hstring{ text.substr(start, safeLength) });
        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::Flush();
    }

    void MainWindow::TargetCutMenuItem_Click(
        winrt::Windows::Foundation::IInspectable const& sender,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
    {
        auto const text = std::wstring{ TargetTextBox().Text().c_str() };
        auto const start = static_cast<size_t>((std::max)(0, TargetTextBox().SelectionStart()));
        auto const length = static_cast<size_t>((std::max)(0, TargetTextBox().SelectionLength()));
        if (length == 0 || start >= text.size())
            return;

        TargetCopyMenuItem_Click(sender, args);

        auto const safeLength = (std::min)(length, text.size() - start);
        auto updated = text.substr(0, start) + text.substr(start + safeLength);
        TargetTextBox().Text(winrt::hstring{ updated });
        TargetTextBox().SelectionStart(static_cast<int32_t>(start));
        TargetTextBox().SelectionLength(0);
        TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    void MainWindow::TargetPasteMenuItem_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto weakThis = get_weak();

        [weakThis]() -> winrt::fire_and_forget
        {
            auto self = weakThis.get();
            if (!self)
                co_return;

            try
            {
                auto const content =
                    winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();
                if (!content.Contains(
                    winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text()))
                {
                    co_return;
                }

                auto const pasted = co_await content.GetTextAsync();
                self = weakThis.get();
                if (!self)
                    co_return;

                auto const text = std::wstring{ self->TargetTextBox().Text().c_str() };
                auto const start = static_cast<size_t>(
                    (std::max)(0, self->TargetTextBox().SelectionStart()));
                auto const length = static_cast<size_t>(
                    (std::max)(0, self->TargetTextBox().SelectionLength()));
                auto const safeStart = (std::min)(start, text.size());
                auto const safeLength = (std::min)(length, text.size() - safeStart);

                auto updated = text.substr(0, safeStart) +
                    std::wstring{ pasted.c_str() } +
                    text.substr(safeStart + safeLength);

                self->TargetTextBox().Text(winrt::hstring{ updated });
                self->TargetTextBox().SelectionStart(static_cast<int32_t>(
                    safeStart + pasted.size()));
                self->TargetTextBox().SelectionLength(0);
                self->TargetTextBox().Focus(
                    winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            }
            catch (...) {}
        }();
    }

    void MainWindow::SetDirty(bool dirty)
    {
        m_hasUnsavedChanges = dirty;
        Title(dirty
            ? L"SRTune *"
            : L"SRTune");
    }

    bool MainWindow::ConfirmSaveBefore(std::wstring const& action)
    {
        if (!m_hasUnsavedChanges)
        {
            if (!m_targetPath.empty())
                SaveWorkspaceState();
            return true;
        }

        auto const message =
            L"Projekt obsahuje neulo\u017Een\u00E9 zm\u011Bny.\n\nChcete je ulo\u017Eit p\u0159ed " + action + L"?";
        auto const result = MessageBoxW(
            GetActiveWindow(),
            message.c_str(),
            L"SRTune",
            MB_YESNOCANCEL | MB_ICONWARNING);

        if (result == IDCANCEL)
        {
            return false;
        }
        if (result == IDNO)
        {
            DeleteWorkspaceDraft();
            return true;
        }

        std::wstring destination;
        if (m_targetPath.empty() && !SelectSubtitleSaveFile(destination))
            return false;

        std::wstring errorMessage;
        if (SaveTargetSubtitleFile(errorMessage, destination))
        {
            return true;
        }

        if (m_lastSaveDetectedExternalChange)
            return OfferSaveAsForExternalChange(errorMessage);

        MessageBoxW(
            GetActiveWindow(),
            errorMessage.c_str(),
            L"Ulo\u017Een\u00ED se nezda\u0159ilo",
            MB_OK | MB_ICONERROR);
        return false;
    }

    bool MainWindow::OfferSaveAsForExternalChange(std::wstring const& errorMessage)
    {
        auto message = errorMessage +
            L"\n\nChcete svou rozpracovanou verzi ulo\u017Eit pod jin\u00FDm n\u00E1zvem?";
        if (MessageBoxW(GetActiveWindow(), message.c_str(), L"Soubor se mezit\u00EDm zm\u011Bnil",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1) != IDYES)
        {
            return false;
        }

        std::wstring destination;
        if (!SelectSubtitleSaveFile(destination))
            return false;

        std::wstring saveError;
        if (!SaveTargetSubtitleFile(saveError, destination))
        {
            MessageBoxW(GetActiveWindow(), saveError.c_str(), L"Ulo\u017Een\u00ED se nezda\u0159ilo",
                MB_OK | MB_ICONERROR);
            return false;
        }

        auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
        auto const backupInfo = m_lastSaveCreatedBackup
            ? std::wstring{ L" \u00B7 z\u00E1loha v LocalAppData" }
            : std::wstring{};
        StatusBarText().Text(hstring{
            L"Rozpracovan\u00E1 verze ulo\u017Eena jako \u00B7 " + targetName + backupInfo });
        return true;
    }

    void MainWindow::HookWindowClosing()
    {
        if (m_windowClosingHookInstalled)
        {
            return;
        }

        m_windowClosingHookInstalled = true;
        AppWindow().Closing([this](
            Microsoft::UI::Windowing::AppWindow const&,
            Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
        {
            if (!ConfirmSaveBefore(L"zav\u0159en\u00EDm aplikace"))
            {
                args.Cancel(true);
            }
        });
    }

    void MainWindow::StartExternalChangeMonitoring()
    {
        if (m_externalChangeTimer)
            return;
        auto const queue = DispatcherQueue();
        if (!queue)
            return;
        m_externalChangeTimer = queue.CreateTimer();
        m_externalChangeTimer.Interval(std::chrono::seconds(3));
        m_externalChangeTimer.IsRepeating(true);
        m_externalChangeTimer.Tick([this](auto const&, auto const&)
        {
            CheckForExternalTargetChange();
        });
        m_externalChangeTimer.Start();
    }

    void MainWindow::CheckForExternalTargetChange()
    {
        if (m_targetPath.empty() || !m_hasTargetFileFingerprint || m_externalChangeAcknowledged)
            return;
        uintmax_t currentSize{};
        int64_t currentTimestamp{};
        auto const targetPath = std::filesystem::path(m_targetPath.c_str());
        if (FileFingerprint(targetPath, currentSize, currentTimestamp) &&
            currentSize == m_targetFileSize && currentTimestamp == m_targetFileTimestamp)
        {
            return;
        }

        m_externalChangeAcknowledged = true;
        auto const result = MessageBoxW(GetActiveWindow(),
            L"Otev\u0159en\u00FD soubor p\u0159ekladu zm\u011Bnila jin\u00E1 aplikace.\n\n"
            L"Ano = na\u010D\u00EDst zm\u011Bn\u011Bn\u00FD soubor\n"
            L"Ne = ulo\u017Eit rozpracovanou verzi pod jin\u00FDm n\u00E1zvem\n"
            L"Storno = pokra\u010Dovat bez na\u010Dten\u00ED",
            L"Soubor se mezit\u00EDm zm\u011Bnil", MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON3);
        if (result == IDYES)
        {
            if (m_hasUnsavedChanges && MessageBoxW(GetActiveWindow(),
                    L"Na\u010Dten\u00ED extern\u00ED verze zahod\u00ED aktu\u00E1ln\u00ED neulo\u017Een\u00E9 zm\u011Bny. Pokra\u010Dovat?",
                    L"Zahodit neulo\u017Een\u00E9 zm\u011Bny?", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
            {
                m_forceSaveAsForRecoveredDraft = true;
                StatusBarText().Text(L"Extern\u00ED zm\u011Bna nen\u00ED na\u010Dtena \u00B7 pou\u017Eijte Ulo\u017Eit jako");
                return;
            }

            std::vector<SubtitleEntry> entries;
            std::wstring errorMessage;
            if (!ReadSubtitleFile(m_targetPath.c_str(), entries, errorMessage))
            {
                MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Extern\u00ED zm\u011Bnu nelze na\u010D\u00EDst",
                    MB_OK | MB_ICONERROR);
                m_forceSaveAsForRecoveredDraft = true;
                return;
            }
            DeleteWorkspaceDraft();
            m_targetEntries = std::move(entries);
            RefreshLoadedProject();
            StatusBarText().Text(L"Extern\u011B zm\u011Bn\u011Bn\u00FD soubor p\u0159ekladu byl znovu na\u010Dten");
            return;
        }

        m_forceSaveAsForRecoveredDraft = true;
        if (result == IDNO)
        {
            SaveAsFromShortcut();
            return;
        }
        StatusBarText().Text(L"Pokra\u010Dujete nad star\u0161\u00ED verz\u00ED \u00B7 dal\u0161\u00ED ulo\u017Een\u00ED mus\u00ED b\u00FDt Ulo\u017Eit jako");
    }

    bool MainWindow::LoadRecentProjectPaths(std::wstring& source, std::wstring& target) const
    {
        source.clear();
        target.clear();
        auto const path = WorkspaceRecentProjectPath();
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return false;

        std::string line;
        if (!std::getline(stream, line) || line != "AEGISUB-WINUI-RECENT\t1")
            return false;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.rfind("SOURCE\t", 0) == 0)
                source = ToWide(UnescapeBridgeField(line.substr(7)));
            else if (line.rfind("TARGET\t", 0) == 0)
                target = ToWide(UnescapeBridgeField(line.substr(7)));
        }
        return !source.empty() && !target.empty() &&
            std::filesystem::exists(source) && std::filesystem::exists(target) &&
            !PathsReferToSameFile(source, target);
    }

    void MainWindow::SaveRecentProjectPaths() const
    {
        if (m_sourcePath.empty() || m_targetPath.empty())
            return;
        auto const path = WorkspaceRecentProjectPath();
        if (path.empty())
            return;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            return;
        auto tempPath = path;
        tempPath += L".tmp-" + std::to_wstring(GetCurrentProcessId());
        std::filesystem::remove(tempPath, error);
        {
            std::ofstream stream(tempPath, std::ios::binary | std::ios::trunc);
            if (!stream)
                return;
            stream << "AEGISUB-WINUI-RECENT\t1\n";
            stream << "SOURCE\t" << EscapeBridgeField(to_string(m_sourcePath)) << '\n';
            stream << "TARGET\t" << EscapeBridgeField(to_string(m_targetPath)) << '\n';
            if (!stream)
                return;
        }
        if (!MoveFileExW(tempPath.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(tempPath, error);
        }
    }

    void MainWindow::RefreshRecentProjectAction()
    {
        std::wstring source;
        std::wstring target;
        bool const available = LoadRecentProjectPaths(source, target);
        RecentProjectMenuItem().IsEnabled(available);
        if (available)
        {
            ToolTipService::SetToolTip(RecentProjectMenuItem(), box_value(hstring{
                std::filesystem::path(source).filename().wstring() + L" + " +
                std::filesystem::path(target).filename().wstring() }));
        }
    }

    void MainWindow::OpenRecentProject()
    {
        std::wstring sourceFilename;
        std::wstring targetFilename;
        if (!LoadRecentProjectPaths(sourceFilename, targetFilename))
        {
            RefreshRecentProjectAction();
            MessageBoxW(GetActiveWindow(), L"Naposledy pou\u017Eit\u00E9 soubory ji\u017E nejsou dostupn\u00E9.",
                L"Posledn\u00ED projekt", MB_OK | MB_ICONINFORMATION);
            return;
        }
        if (!ConfirmSaveBefore(L"otev\u0159en\u00EDm posledn\u00EDho projektu"))
            return;

        std::vector<SubtitleEntry> sourceEntries;
        std::vector<SubtitleEntry> targetEntries;
        std::wstring errorMessage;
        if (!ReadSubtitleFile(sourceFilename, sourceEntries, errorMessage) ||
            !ReadSubtitleFile(targetFilename, targetEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Posledn\u00ED projekt nelze otev\u0159\u00EDt",
                MB_OK | MB_ICONERROR);
            RefreshRecentProjectAction();
            return;
        }
        m_sourceEntries = std::move(sourceEntries);
        m_targetEntries = std::move(targetEntries);
        m_sourcePath = hstring{ sourceFilename };
        m_targetPath = hstring{ targetFilename };
        m_transcriptEntries.clear();
        m_transcriptChunks.clear();
        m_transcriptPath = L"";
        m_projectPath.clear();
        RefreshLoadedProject();
    }

    bool MainWindow::SelectSubtitleFile(std::wstring const& title, std::wstring& filename) const
    {
        wchar_t buffer[32768]{};
        wchar_t const filter[] =
            L"Titulky SubRip (*.srt)\0*.srt\0"
            L"V\u0161echny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = title.c_str();
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&dialog))
        {
            return false;
        }

        filename = buffer;
        return true;
    }

    bool MainWindow::SelectSubtitleSaveFile(std::wstring& filename) const
    {
        wchar_t buffer[32768]{};
        std::wstring suggestedName;
        if (!m_targetPath.empty())
        {
            suggestedName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
        }
        else if (!m_sourcePath.empty())
        {
            auto source = std::filesystem::path(m_sourcePath.c_str());
            suggestedName = source.stem().wstring() + L".cs" + source.extension().wstring();
        }
        if (!suggestedName.empty())
        {
            wcsncpy_s(buffer, suggestedName.c_str(), _TRUNCATE);
        }

        wchar_t const filter[] =
            L"Titulky SubRip (*.srt)\0*.srt\0"
            L"V\u0161echny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Ulo\u017Eit p\u0159eklad jako";
        dialog.lpstrDefExt = L"srt";
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

        if (!GetSaveFileNameW(&dialog))
            return false;

        filename = buffer;
        return true;
    }

    bool MainWindow::SelectProjectFile(std::wstring& filename) const
    {
        wchar_t buffer[32768]{};
        wchar_t const filter[] =
            L"SRTune projekt (*.srtuneproj)\0*.srtuneproj\0"
            L"Všechny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Otevřít projekt SRTune";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&dialog))
            return false;

        filename = buffer;
        return true;
    }

    bool MainWindow::SelectProjectSaveFile(std::wstring& filename) const
    {
        wchar_t buffer[32768]{};

        std::wstring suggested = L"project.srtuneproj";
        if (!m_targetPath.empty())
        {
            suggested = std::filesystem::path(m_targetPath.c_str()).stem().wstring() +
                L".srtuneproj";
        }
        else if (!m_sourcePath.empty())
        {
            suggested = std::filesystem::path(m_sourcePath.c_str()).stem().wstring() +
                L".srtuneproj";
        }
        wcsncpy_s(buffer, suggested.c_str(), _TRUNCATE);

        wchar_t const filter[] =
            L"SRTune projekt (*.srtuneproj)\0*.srtuneproj\0"
            L"Všechny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Uložit projekt SRTune";
        dialog.lpstrDefExt = L"srtuneproj";
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

        if (!GetSaveFileNameW(&dialog))
            return false;

        filename = buffer;
        return true;
    }

    bool MainWindow::SaveProjectFile(bool saveAs)
    {
        std::wstring destination = m_projectPath;
        if (saveAs || destination.empty())
        {
            if (!SelectProjectSaveFile(destination))
                return false;
        }

        auto const projectFile = std::filesystem::absolute(
            std::filesystem::path(destination)).lexically_normal();
        auto tempFile = projectFile;
        tempFile += L".tmp-" + std::to_wstring(GetCurrentProcessId());

        std::error_code error;
        std::filesystem::remove(tempFile, error);

        std::ofstream stream(tempFile, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            MessageBoxW(GetActiveWindow(), L"Projektový soubor nelze vytvořit.",
                L"Uložení projektu se nezdařilo", MB_OK | MB_ICONERROR);
            return false;
        }

        auto writePath = [&](char const* key, std::wstring_view value)
        {
            auto const stored = StoreProjectPath(projectFile, value);
            stream << key << '\t'
                << EscapeBridgeField(winrt::to_string(winrt::hstring{ stored })) << '\n';
        };

        stream << "SRTUNE-PROJECT\t1\n";
        writePath("SOURCE", std::wstring_view{ m_sourcePath.c_str(), m_sourcePath.size() });
        writePath("TARGET", std::wstring_view{ m_targetPath.c_str(), m_targetPath.size() });
        writePath("MEDIA", m_videoPath);
        writePath("WAVEFORM", m_waveformPath);
        writePath("TRANSCRIPT", std::wstring_view{ m_transcriptPath.c_str(), m_transcriptPath.size() });
        writePath("GLOSSARY", m_glossaryPath);
        stream << "CURRENT\t" << m_currentIndex << '\n';

        stream.close();
        if (!stream)
        {
            std::filesystem::remove(tempFile, error);
            MessageBoxW(GetActiveWindow(), L"Projektový soubor se nepodařilo zapsat.",
                L"Uložení projektu se nezdařilo", MB_OK | MB_ICONERROR);
            return false;
        }

        if (!MoveFileExW(tempFile.c_str(), projectFile.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(tempFile, error);
            MessageBoxW(GetActiveWindow(), L"Projektový soubor se nepodařilo dokončit.",
                L"Uložení projektu se nezdařilo", MB_OK | MB_ICONERROR);
            return false;
        }

        m_projectPath = projectFile.wstring();
        StatusBarText().Text(winrt::hstring{
            L"Projekt uložen · " + projectFile.filename().wstring() });
        return true;
    }

    void MainWindow::OpenProjectFile()
    {
        std::wstring filename;
        if (!SelectProjectFile(filename))
            return;

        if (!ConfirmSaveBefore(L"otevřením jiného projektu"))
            return;

        auto const projectFile = std::filesystem::absolute(
            std::filesystem::path(filename)).lexically_normal();

        std::ifstream stream(projectFile, std::ios::binary);
        if (!stream)
        {
            MessageBoxW(GetActiveWindow(), L"Projektový soubor nelze otevřít.",
                L"Projekt nelze otevřít", MB_OK | MB_ICONERROR);
            return;
        }

        std::string line;
        if (!std::getline(stream, line) || line != "SRTUNE-PROJECT\t1")
        {
            MessageBoxW(GetActiveWindow(), L"Soubor není platný projekt SRTune.",
                L"Projekt nelze otevřít", MB_OK | MB_ICONERROR);
            return;
        }

        std::map<std::string, std::wstring> values;
        int32_t savedIndex = 0;

        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto const tab = line.find('\t');
            if (tab == std::string::npos)
                continue;

            auto const key = line.substr(0, tab);
            auto const value = line.substr(tab + 1);
            if (key == "CURRENT")
            {
                try { savedIndex = std::stoi(value); }
                catch (...) { savedIndex = 0; }
            }
            else
            {
                values[key] = ResolveProjectPath(
                    projectFile, ToWide(UnescapeBridgeField(value)));
            }
        }

        auto getPath = [&](char const* key) -> std::wstring
        {
            auto const found = values.find(key);
            return found == values.end() ? std::wstring{} : found->second;
        };

        auto const sourcePath = getPath("SOURCE");
        auto const targetPath = getPath("TARGET");
        auto const mediaPath = getPath("MEDIA");
        auto const waveformPath = getPath("WAVEFORM");
        auto const transcriptPath = getPath("TRANSCRIPT");
        auto const glossaryPath = getPath("GLOSSARY");

        if (!sourcePath.empty() && !targetPath.empty() &&
            PathsReferToSameFile(sourcePath, targetPath))
        {
            ShowSameSubtitleFileWarning();
            return;
        }

        std::vector<SubtitleEntry> sourceEntries;
        std::vector<SubtitleEntry> targetEntries;
        std::vector<SubtitleEntry> transcriptEntries;
        std::vector<winrt::hstring> transcriptChunks;
        std::wstring errorMessage;

        if (!sourcePath.empty() &&
            !ReadSubtitleFile(sourcePath, sourceEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(),
                L"Originál projektu nelze otevřít", MB_OK | MB_ICONERROR);
            return;
        }

        if (!targetPath.empty() &&
            !ReadSubtitleFile(targetPath, targetEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(),
                L"Překlad projektu nelze otevřít", MB_OK | MB_ICONERROR);
            return;
        }

        if (!transcriptPath.empty() &&
            !ReadTranscriptFile(
                transcriptPath, transcriptEntries, transcriptChunks, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(),
                L"Transcript projektu nelze otevřít", MB_OK | MB_ICONERROR);
            return;
        }

        ResetWorkspaceToBlank();

        m_sourceEntries = std::move(sourceEntries);
        m_targetEntries = std::move(targetEntries);
        m_transcriptEntries = std::move(transcriptEntries);
        m_transcriptChunks = std::move(transcriptChunks);
        m_sourcePath = winrt::hstring{ sourcePath };
        m_targetPath = winrt::hstring{ targetPath };
        m_transcriptPath = winrt::hstring{ transcriptPath };
        m_projectPath = projectFile.wstring();

        RefreshLoadedProject();

        if (!mediaPath.empty() && std::filesystem::exists(mediaPath))
            OpenVideoFile(mediaPath);

        if (!waveformPath.empty() && std::filesystem::exists(waveformPath) &&
            (mediaPath.empty() || !PathsReferToSameFile(mediaPath, waveformPath)))
        {
            LoadWaveformForMedia(waveformPath);
        }

        if (!glossaryPath.empty() && std::filesystem::exists(glossaryPath))
            LoadGlossaryFromFile(glossaryPath);

        if (!m_rows.empty())
        {
            m_currentIndex = (std::max)(0,
                (std::min)(static_cast<int32_t>(m_rows.size()) - 1, savedIndex));
            m_selectedSubtitleIndices.assign(1, m_currentIndex);
            m_selectionAnchorIndex = m_currentIndex;
            LoadCurrentRow();
        }

        RefreshProjectFileLabels();
        StatusBarText().Text(winrt::hstring{
            L"Projekt otevřen · " + projectFile.filename().wstring() });
    }

    void MainWindow::OpenProjectFiles()
    {
        if (!ConfirmSaveBefore(L"otev\u0159en\u00EDm jin\u00E9ho projektu"))
        {
            return;
        }

        std::wstring sourceFilename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt origin\u00E1ln\u00ED titulky", sourceFilename))
        {
            return;
        }

        std::vector<SubtitleEntry> sourceEntries;
        std::wstring errorMessage;
        if (!ReadSubtitleFile(sourceFilename, sourceEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"SRTune", MB_OK | MB_ICONERROR);
            return;
        }

        std::wstring targetFilename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt p\u0159eklad", targetFilename))
        {
            return;
        }
        if (PathsReferToSameFile(sourceFilename, targetFilename))
        {
            ShowSameSubtitleFileWarning();
            return;
        }

        std::vector<SubtitleEntry> targetEntries;
        if (!ReadSubtitleFile(targetFilename, targetEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"SRTune", MB_OK | MB_ICONERROR);
            return;
        }

        m_sourceEntries = std::move(sourceEntries);
        m_sourcePath = hstring{ sourceFilename };
        m_targetEntries = std::move(targetEntries);
        m_targetPath = hstring{ targetFilename };
        m_transcriptEntries.clear();
        m_transcriptChunks.clear();
        m_transcriptPath = L"";
        m_projectPath.clear();
        RefreshLoadedProject();
    }

    void MainWindow::OpenSourceFile()
    {
        if (!ConfirmSaveBefore(L"otev\u0159en\u00EDm jin\u00E9ho origin\u00E1lu"))
            return;

        std::wstring filename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt origin\u00E1ln\u00ED titulky", filename))
            return;
        if (PathsReferToSameFile(filename, std::wstring_view{ m_targetPath.c_str(), m_targetPath.size() }))
        {
            ShowSameSubtitleFileWarning();
            return;
        }

        std::vector<SubtitleEntry> entries;
        std::wstring errorMessage;
        if (!ReadSubtitleFile(filename, entries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"SRTune", MB_OK | MB_ICONERROR);
            return;
        }

        std::vector<SubtitleEntry> refreshedTargetEntries;
        if (!m_targetPath.empty()
            && !ReadSubtitleFile(m_targetPath.c_str(), refreshedTargetEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"SRTune", MB_OK | MB_ICONERROR);
            return;
        }

        m_sourceEntries = std::move(entries);
        m_sourcePath = hstring{ filename };
        if (!m_targetPath.empty())
            m_targetEntries = std::move(refreshedTargetEntries);
        RefreshLoadedProject();
    }

    void MainWindow::OpenTargetFile()
    {
        if (!ConfirmSaveBefore(L"otev\u0159en\u00EDm jin\u00E9ho p\u0159ekladu"))
            return;

        std::wstring filename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt p\u0159eklad", filename))
            return;
        if (PathsReferToSameFile(filename, std::wstring_view{ m_sourcePath.c_str(), m_sourcePath.size() }))
        {
            ShowSameSubtitleFileWarning();
            return;
        }

        std::vector<SubtitleEntry> entries;
        std::wstring errorMessage;
        if (!ReadSubtitleFile(filename, entries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"SRTune", MB_OK | MB_ICONERROR);
            return;
        }

        m_targetEntries = std::move(entries);
        m_targetPath = hstring{ filename };
        RefreshLoadedProject();
    }

    bool MainWindow::ReadTranscriptTextFile(
        std::wstring const& filename,
        std::vector<winrt::hstring>& chunks,
        std::wstring& errorMessage) const
    {
        std::ifstream stream(filename, std::ios::binary);
        if (!stream)
        {
            errorMessage = L"Soubor transcriptu nelze otevřít.";
            return false;
        }

        std::string bytes{
            std::istreambuf_iterator<char>{ stream },
            std::istreambuf_iterator<char>{} };

        auto text = TrimTranscriptText(DecodeTextBytes(std::move(bytes)));
        if (text.empty())
        {
            errorMessage = L"Transcript je prázdný.";
            return false;
        }

        chunks = ChunkTranscriptText(std::move(text));
        if (chunks.empty())
        {
            errorMessage = L"Transcript neobsahuje použitelný text.";
            return false;
        }
        return true;
    }

    bool MainWindow::ReadTranscriptDocxFile(
        std::wstring const& filename,
        std::vector<winrt::hstring>& chunks,
        std::wstring& errorMessage) const
    {
        auto const tempRoot = std::filesystem::temp_directory_path() /
            (L"srtune-docx-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(GetTickCount64()));
        auto const archivePath = tempRoot / L"transcript.zip";
        auto const extractPath = tempRoot / L"expanded";

        std::error_code error;
        std::filesystem::create_directories(extractPath, error);
        if (error)
        {
            errorMessage = L"Nepodařilo se vytvořit dočasnou složku pro DOCX.";
            return false;
        }

        auto cleanup = [&]()
        {
            std::error_code cleanupError;
            std::filesystem::remove_all(tempRoot, cleanupError);
        };

        std::filesystem::copy_file(
            filename, archivePath,
            std::filesystem::copy_options::overwrite_existing, error);
        if (error)
        {
            cleanup();
            errorMessage = L"DOCX se nepodařilo připravit k načtení.";
            return false;
        }

        std::wstring command =
            L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "
            L"\"Expand-Archive -LiteralPath " + PowerShellQuoted(archivePath.wstring()) +
            L" -DestinationPath " + PowerShellQuoted(extractPath.wstring()) + L" -Force\"";

        DWORD exitCode = 0;
        if (!RunProcess(command, exitCode) || exitCode != 0)
        {
            cleanup();
            errorMessage = L"DOCX se nepodařilo rozbalit. Zkontrolujte, že je soubor platný.";
            return false;
        }

        auto const documentPath = extractPath / L"word" / L"document.xml";
        std::ifstream xmlStream(documentPath, std::ios::binary);
        if (!xmlStream)
        {
            cleanup();
            errorMessage = L"DOCX neobsahuje očekávaný dokument.xml.";
            return false;
        }

        std::string xml{
            std::istreambuf_iterator<char>{ xmlStream },
            std::istreambuf_iterator<char>{} };
        xmlStream.close();

        std::wstring text;
        size_t paragraphSearch = 0;
        while (true)
        {
            auto const paragraphStart = xml.find("<w:p", paragraphSearch);
            if (paragraphStart == std::string::npos)
                break;
            auto const paragraphOpenEnd = xml.find('>', paragraphStart);
            auto const paragraphEnd = xml.find("</w:p>", paragraphOpenEnd);
            if (paragraphOpenEnd == std::string::npos || paragraphEnd == std::string::npos)
                break;

            std::wstring paragraph;
            size_t runSearch = paragraphOpenEnd + 1;
            while (runSearch < paragraphEnd)
            {
                auto const textStart = xml.find("<w:t", runSearch);
                if (textStart == std::string::npos || textStart >= paragraphEnd)
                    break;
                auto const textOpenEnd = xml.find('>', textStart);
                auto const textEnd = xml.find("</w:t>", textOpenEnd);
                if (textOpenEnd == std::string::npos || textEnd == std::string::npos ||
                    textEnd > paragraphEnd)
                {
                    break;
                }

                auto const raw = xml.substr(textOpenEnd + 1, textEnd - textOpenEnd - 1);
                paragraph += DecodeXmlEntities(DecodeTextBytes(raw));
                runSearch = textEnd + 6;
            }

            paragraph = TrimTranscriptText(std::move(paragraph));
            if (!paragraph.empty())
            {
                if (!text.empty())
                    text += L"\n\n";
                text += paragraph;
            }

            paragraphSearch = paragraphEnd + 6;
        }

        cleanup();

        if (text.empty())
        {
            errorMessage = L"V DOCX nebyl nalezen žádný text.";
            return false;
        }

        chunks = ChunkTranscriptText(std::move(text));
        if (chunks.empty())
        {
            errorMessage = L"V DOCX nebyl nalezen použitelný transcript.";
            return false;
        }
        return true;
    }

    bool MainWindow::ReadTranscriptFile(
        std::wstring const& filename,
        std::vector<SubtitleEntry>& timedEntries,
        std::vector<winrt::hstring>& chunks,
        std::wstring& errorMessage) const
    {
        timedEntries.clear();
        chunks.clear();

        auto extension = std::filesystem::path(filename).extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });

        if (extension == L".srt")
            return ReadSubtitleFile(filename, timedEntries, errorMessage);
        if (extension == L".txt")
            return ReadTranscriptTextFile(filename, chunks, errorMessage);
        if (extension == L".docx")
            return ReadTranscriptDocxFile(filename, chunks, errorMessage);
        if (extension == L".doc")
        {
            errorMessage =
                L"Starý binární formát DOC nelze bezpečně číst bez Microsoft Word. "
                L"Uložte přepis jako DOCX nebo TXT.";
            return false;
        }

        errorMessage = L"Podporované formáty transcriptu jsou TXT, DOCX a SRT.";
        return false;
    }

    void MainWindow::OpenTranscriptFile()
    {
        wchar_t buffer[32768]{};
        wchar_t const filter[] =
            L"Transcript (*.txt;*.docx;*.srt)\0*.txt;*.docx;*.srt\0"
            L"Text (*.txt)\0*.txt\0"
            L"Word dokument (*.docx)\0*.docx\0"
            L"Časovaný transcript (*.srt)\0*.srt\0"
            L"Všechny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Otevřít kontext / transcript";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&dialog))
            return;

        std::wstring filename{ buffer };
        std::vector<SubtitleEntry> timedEntries;
        std::vector<winrt::hstring> chunks;
        std::wstring errorMessage;

        if (!ReadTranscriptFile(filename, timedEntries, chunks, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(),
                L"Transcript nelze otevřít", MB_OK | MB_ICONERROR);
            return;
        }

        m_transcriptEntries = std::move(timedEntries);
        m_transcriptChunks = std::move(chunks);
        m_transcriptPath = hstring{ filename };
        RefreshProjectFileLabels();
        RefreshTranscriptContext();

        auto const mode = m_transcriptChunks.empty()
            ? std::wstring{ L"časovaný SRT" }
            : std::wstring{ L"textový transcript · " } +
                std::to_wstring(m_transcriptChunks.size()) + L" částí";

        StatusBarText().Text(hstring{
            L"Kontext / transcript: " +
            std::filesystem::path(filename).filename().wstring() +
            L" · " + mode });
    }

    void MainWindow::RefreshLoadedProject()
    {
        m_externalChangeAcknowledged = false;
        ClearBulkUndo();
        ClearWorkspaceHistory();
        BuildAlignedRows();
        m_selectedSubtitleIndices.clear();
        if (!m_rows.empty())
            m_selectedSubtitleIndices.push_back(m_currentIndex);
        m_selectionAnchorIndex = m_currentIndex;
        m_waveformViewportSubtitleIndex = -1;
        LoadWorkspaceState();
        m_forceSaveAsForRecoveredDraft = false;
        bool const restoredDraft = LoadWorkspaceDraft();
        InitializeWorkflowStatuses();
        RefreshQaAll();
        RebuildSubtitleGrid();
        LoadCurrentRow();
        RefreshCurrentQaVisuals();
        RefreshProjectFileLabels();
        RefreshSearchSummary();
        if (restoredDraft)
            UpdateDirtyFromRows();
        else
            SetDirty(false);
        if (!m_forceSaveAsForRecoveredDraft)
        {
            m_hasTargetFileFingerprint = !m_targetPath.empty() && FileFingerprint(
                std::filesystem::path(m_targetPath.c_str()), m_targetFileSize, m_targetFileTimestamp);
        }
        if (!m_targetPath.empty())
        {
            auto const backupPath = WorkspaceBackupPath(std::filesystem::path(m_targetPath.c_str()));
            if (!backupPath.empty())
                CleanupWorkspaceBackups(backupPath.parent_path(), {});
            auto const draftPath = WorkspaceDraftPath(m_targetPath);
            if (!draftPath.empty())
                CleanupWorkspaceDrafts(draftPath.parent_path(), draftPath);
        }
        if (!m_sourceEntries.empty() && !m_targetEntries.empty() &&
            m_sourceEntries.size() != m_targetEntries.size())
        {
            StatusBarText().Text(hstring{
                L"Pozor: origin\u00E1l " + std::to_wstring(m_sourceEntries.size()) +
                L" titulk\u016F \u00B7 p\u0159eklad " + std::to_wstring(m_targetEntries.size()) +
                L" \u00B7 zkontrolujte p\u00E1rov\u00E1n\u00ED podle \u010Dasu" });
        }
        if (restoredDraft)
            StatusBarText().Text(L"Obnoven neulo\u017Een\u00FD pracovn\u00ED koncept");
        SaveRecentProjectPaths();
        RefreshRecentProjectAction();
    }

    void MainWindow::LoadWorkspaceState()
    {
        m_workflowStateDirty = false;
        auto const statePath = WorkspaceStatePath(m_targetPath);
        if (statePath.empty() || m_targetPath.empty())
            return;

        uintmax_t currentSize{};
        int64_t currentTimestamp{};
        if (!FileFingerprint(std::filesystem::path(m_targetPath.c_str()), currentSize, currentTimestamp))
            return;

        std::ifstream stream(statePath, std::ios::binary);
        if (!stream)
            return;

        std::string line;
        if (!std::getline(stream, line) || line != "AEGISUB-WINUI-STATE\t1")
            return;
        if (!std::getline(stream, line) || line.rfind("FILE\t", 0) != 0)
            return;

        auto const sizeSeparator = line.find('\t', 5);
        if (sizeSeparator == std::string::npos)
            return;
        try
        {
            auto const savedSize = static_cast<uintmax_t>(std::stoull(line.substr(5, sizeSeparator - 5)));
            auto const savedTimestamp = std::stoll(line.substr(sizeSeparator + 1));
            if (savedSize != currentSize || savedTimestamp != currentTimestamp)
                return;
        }
        catch (...)
        {
            return;
        }

        int32_t restoredIndex = 0;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            try
            {
                if (line.rfind("CURRENT\t", 0) == 0)
                {
                    restoredIndex = std::stoi(line.substr(8));
                    continue;
                }
                if (line.rfind("ROW\t", 0) != 0)
                    continue;
                auto const separator = line.find('\t', 4);
                if (separator == std::string::npos)
                    continue;
                auto const index = std::stoi(line.substr(4, separator - 4));
                if (index < 0 || index >= static_cast<int32_t>(m_rows.size()))
                    continue;
                m_rows[index].workflowStatus = to_hstring(UnescapeBridgeField(line.substr(separator + 1)));
                m_rows[index].status = m_rows[index].workflowStatus;
                m_rows[index].savedWorkflowStatus = m_rows[index].workflowStatus;
            }
            catch (...)
            {
            }
        }

        if (!m_rows.empty())
            m_currentIndex = (std::max)(0, (std::min)(static_cast<int32_t>(m_rows.size()) - 1, restoredIndex));
    }

    bool MainWindow::SaveWorkspaceState()
    {
        auto const statePath = WorkspaceStatePath(m_targetPath);
        if (statePath.empty() || m_targetPath.empty())
            return false;

        uintmax_t fileSize{};
        int64_t fileTimestamp{};
        if (!FileFingerprint(std::filesystem::path(m_targetPath.c_str()), fileSize, fileTimestamp))
            return false;

        std::error_code error;
        std::filesystem::create_directories(statePath.parent_path(), error);
        if (error)
            return false;

        auto tempPath = statePath;
        tempPath += L".tmp-" + std::to_wstring(GetCurrentProcessId());
        std::filesystem::remove(tempPath, error);
        {
            std::ofstream stream(tempPath, std::ios::binary | std::ios::trunc);
            if (!stream)
                return false;
            stream << "AEGISUB-WINUI-STATE\t1\n";
            stream << "FILE\t" << fileSize << '\t' << fileTimestamp << '\n';
            stream << "CURRENT\t" << m_currentIndex << '\n';
            for (size_t index = 0; index < m_rows.size(); ++index)
            {
                auto const& status = m_rows[index].workflowStatus.empty()
                    ? m_rows[index].status
                    : m_rows[index].workflowStatus;
                stream << "ROW\t" << index << '\t'
                    << EscapeBridgeField(to_string(status)) << '\n';
            }
            if (!stream)
                return false;
        }

        if (!MoveFileExW(tempPath.c_str(), statePath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(tempPath, error);
            return false;
        }
        return true;
    }

    bool MainWindow::LoadWorkspaceDraft()
    {
        auto const draftPath = WorkspaceDraftPath(m_targetPath);
        if (draftPath.empty() || m_targetPath.empty())
            return false;

        std::ifstream stream(draftPath, std::ios::binary);
        if (!stream)
            return false;

        std::string const serialized{
            std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{} };
        agi::winui::RecoveryDraft savedDraft;
        if (!agi::winui::ParseRecoveryDraft(serialized, savedDraft) ||
            savedDraft.row_count != m_rows.size())
            return false;

        uintmax_t currentSize{};
        int64_t currentTimestamp{};
        bool const sameFileVersion = FileFingerprint(
            std::filesystem::path(m_targetPath.c_str()), currentSize, currentTimestamp) &&
            currentSize == savedDraft.file_size && currentTimestamp == savedDraft.file_timestamp;
        auto const message = sameFileVersion
            ? L"Byl nalezen neulo\u017Een\u00FD pracovn\u00ED koncept. Chcete jej obnovit?"
            : L"Byl nalezen koncept ze star\u0161\u00ED verze souboru. Chcete jej obnovit?\n\n"
              L"Kv\u016Fli bezpe\u010Dnosti jej bude mo\u017En\u00E9 ulo\u017Eit pouze pod jin\u00FDm n\u00E1zvem.";
        if (MessageBoxW(GetActiveWindow(), message, L"Obnovit pracovn\u00ED koncept?",
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1) != IDYES)
        {
            DeleteWorkspaceDraft();
            return false;
        }

        for (auto const& draft : savedDraft.rows)
        {
            if (draft.index >= m_rows.size())
                return false;
            auto& row = m_rows[draft.index];
            row.target = to_hstring(draft.target);
            row.targetModified = !agi::winui::EquivalentEditorText(
                row.target.c_str(), row.savedTarget.c_str());
            row.workflowStatus = to_hstring(draft.status);
            row.status = row.targetModified ? hstring{ L"Upraveno" } : row.workflowStatus;
            row.historyInitialized = true;
            if (draft.index < static_cast<int32_t>(m_targetEntries.size()))
                m_targetEntries[draft.index].text = row.target;
        }
        m_workflowStateDirty = savedDraft.workflow_dirty;
        m_currentIndex = m_rows.empty()
            ? 0
            : (std::max)(0, (std::min)(static_cast<int32_t>(m_rows.size()) - 1, savedDraft.current_index));
        m_forceSaveAsForRecoveredDraft = !sameFileVersion;
        if (m_forceSaveAsForRecoveredDraft)
        {
            m_hasTargetFileFingerprint = true;
            m_targetFileSize = savedDraft.file_size;
            m_targetFileTimestamp = savedDraft.file_timestamp;
        }
        return true;
    }

    bool MainWindow::SaveWorkspaceDraft()
    {
        if (!m_hasUnsavedChanges)
        {
            DeleteWorkspaceDraft();
            return true;
        }

        auto const draftPath = WorkspaceDraftPath(m_targetPath);
        if (draftPath.empty() || m_targetPath.empty() || !m_hasTargetFileFingerprint)
            return false;

        std::error_code error;
        std::filesystem::create_directories(draftPath.parent_path(), error);
        if (error)
            return false;

        auto tempPath = draftPath;
        tempPath += L".tmp-" + std::to_wstring(GetCurrentProcessId());
        std::filesystem::remove(tempPath, error);
        agi::winui::RecoveryDraft draft;
        draft.file_size = m_targetFileSize;
        draft.file_timestamp = m_targetFileTimestamp;
        draft.row_count = m_rows.size();
        draft.current_index = m_currentIndex;
        draft.workflow_dirty = m_workflowStateDirty;
        draft.rows.reserve(m_rows.size());
        for (size_t index = 0; index < m_rows.size(); ++index)
        {
            auto const& row = m_rows[index];
            auto const& status = row.workflowStatus.empty() ? row.status : row.workflowStatus;
            draft.rows.push_back({ index, to_string(status), to_string(row.target) });
        }
        {
            std::ofstream stream(tempPath, std::ios::binary | std::ios::trunc);
            if (!stream)
                return false;
            stream << agi::winui::SerializeRecoveryDraft(draft);
            if (!stream)
                return false;
        }

        if (!MoveFileExW(tempPath.c_str(), draftPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(tempPath, error);
            return false;
        }
        return true;
    }

    void MainWindow::DeleteWorkspaceDraft()
    {
        if (m_workspaceDraftTimer)
            m_workspaceDraftTimer.Stop();
        auto const draftPath = WorkspaceDraftPath(m_targetPath);
        if (draftPath.empty())
            return;
        std::error_code error;
        std::filesystem::remove(draftPath, error);
    }

    void MainWindow::ScheduleWorkspaceDraftSave()
    {
        if (!m_initialized || m_targetPath.empty())
            return;
        if (!m_workspaceDraftTimer)
        {
            auto const queue = DispatcherQueue();
            if (!queue)
                return;
            m_workspaceDraftTimer = queue.CreateTimer();
            m_workspaceDraftTimer.Interval(std::chrono::seconds(1));
            m_workspaceDraftTimer.IsRepeating(false);
            m_workspaceDraftTimer.Tick([this](auto const&, auto const&)
            {
                bool const hadUnsavedChanges = m_hasUnsavedChanges;
                if (!SaveWorkspaceDraft())
                {
                    if (hadUnsavedChanges)
                    {
                        StatusBarText().Text(
                            L"Pozor: pracovn\u00ED koncept se nepoda\u0159ilo ulo\u017Eit \u00B7 pou\u017Eijte Ctrl+S");
                    }
                    return;
                }
                if (!hadUnsavedChanges)
                    return;

                auto const draftTime = FormatFileWriteTime(WorkspaceDraftPath(m_targetPath));
                std::wstring status = L"Neulo\u017Een\u00E9 zm\u011Bny \u00B7 pracovn\u00ED koncept bezpe\u010Dn\u011B ulo\u017Een";
                if (!draftTime.empty())
                    status += L" \u00B7 " + draftTime;
                if (m_forceSaveAsForRecoveredDraft)
                    status += L" \u00B7 bude nutn\u00E9 Ulo\u017Eit jako";
                StatusBarText().Text(hstring{ status });
            });
        }
        m_workspaceDraftTimer.Stop();
        m_workspaceDraftTimer.Start();
    }

    void MainWindow::RefreshProjectFileLabels()
    {
        auto update = [](TextBlock const& label, hstring const& path, wchar_t const* emptyText)
        {
            if (path.empty())
            {
                label.Text(emptyText);
                ToolTipService::SetToolTip(label, nullptr);
                return;
            }

            auto const filename = std::filesystem::path(path.c_str()).filename().wstring();
            label.Text(hstring{ filename });
            ToolTipService::SetToolTip(label, box_value(path));
        };

        update(OriginalFileText(), m_sourcePath, L"");
        update(TargetFileText(), m_targetPath, L"");

        if (!m_transcriptPath.empty())
        {
            auto const filename = std::filesystem::path(m_transcriptPath.c_str()).filename().wstring();
            TranscriptFileText().Text(hstring{ filename });
            ToolTipService::SetToolTip(TranscriptFileText(), box_value(m_transcriptPath));
        }
        else if (!m_sourcePath.empty())
        {
            TranscriptFileText().Text(L"zdroj: origin\u00E1l");
            ToolTipService::SetToolTip(TranscriptFileText(), box_value(hstring{
                L"Samostatn\u00FD transcript nen\u00ED na\u010Dten. Kontext se bere z origin\u00E1ln\u00EDch titulk\u016F: " +
                std::wstring{ m_sourcePath.c_str() } }));
        }
        else
        {
            TranscriptFileText().Text(L"bez zdroje");
            ToolTipService::SetToolTip(TranscriptFileText(), box_value(
                L"Na\u010Dt\u011Bte origin\u00E1l nebo samostatn\u00FD \u010Dasovan\u00FD transcript ve form\u00E1tu SRT."));
        }

        RefreshOriginalPanelVisibility();
        RefreshBackupAction();
    }

    void MainWindow::RefreshOriginalPanelVisibility()
    {
        bool const available = !m_sourcePath.empty() && !m_sourceEntries.empty();
        bool const visible = available && !m_originalPanelManuallyHidden;

        OriginalPanelBorder().Visibility(visible ? Visibility::Visible : Visibility::Collapsed);
        if (visible)
        {
            Grid::SetColumn(TargetPanelBorder(), 1);
            Grid::SetColumnSpan(TargetPanelBorder(), 1);
        }
        else
        {
            Grid::SetColumn(TargetPanelBorder(), 0);
            Grid::SetColumnSpan(TargetPanelBorder(), 2);
        }

        auto updateColumns = [visible](Grid const& table)
        {
            auto const columns = table.ColumnDefinitions();
            if (columns.Size() < 5)
                return;
            columns.GetAt(3).Width(visible
                ? GridLength{ 1.1, GridUnitType::Star }
                : GridLength{ 0.0, GridUnitType::Pixel });
            columns.GetAt(4).Width(visible
                ? GridLength{ 1.1, GridUnitType::Star }
                : GridLength{ 2.2, GridUnitType::Star });
        };
        updateColumns(SubtitleColumnHeader());
        updateColumns(SubtitleGridHost());

        ToggleOriginalPanelMenuItem().IsEnabled(available);
        ToggleOriginalPanelMenuItem().Text(visible ? L"Skrýt originál" : L"Zobrazit originál");
    }

    void MainWindow::ToggleOriginalPanelMenuItem_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_sourcePath.empty() || m_sourceEntries.empty())
            return;
        m_originalPanelManuallyHidden = !m_originalPanelManuallyHidden;
        RefreshOriginalPanelVisibility();
        RenderWaveform();
    }

    void MainWindow::RefreshBackupAction()
    {
        bool available = false;
        std::filesystem::path backupPath;
        if (!m_targetPath.empty())
        {
            backupPath = WorkspaceBackupPath(std::filesystem::path(m_targetPath.c_str()));
            available = !backupPath.empty() && std::filesystem::exists(backupPath);
        }

        auto const button = RestoreBackupButton();
        button.IsEnabled(available);
        if (available)
        {
            std::wstring tooltip = L"Obnovit p\u0159edchoz\u00ED ulo\u017Eenou verzi";
            auto const backupTime = FormatFileWriteTime(backupPath);
            if (!backupTime.empty())
                tooltip += L" z " + backupTime;
            tooltip += L"; sou\u010Dasn\u00E1 verze se zachov\u00E1 jako z\u00E1loha";
            ToolTipService::SetToolTip(button, box_value(hstring{ tooltip }));
        }
        else
        {
            ToolTipService::SetToolTip(button, box_value(
                L"Pro otev\u0159en\u00FD soubor p\u0159ekladu zat\u00EDm nen\u00ED dostupn\u00E1 z\u00E1loha"));
        }
    }

    bool MainWindow::IsTranslationEmpty(hstring const& text) const
    {
        std::wstring const value{ text.c_str() };
        return value.empty() || std::all_of(value.begin(), value.end(), [](wchar_t character)
        {
            return std::iswspace(character) != 0;
        });
    }

    bool MainWindow::RowMatchesActiveFilter(SubtitleRowData const& row) const
    {
        return agi::winui::MatchesSubtitleFilter(
            m_activeFilter,
            IsTranslationEmpty(row.target),
            row.targetModified,
            !row.qaIssue.empty(),
            row.workflowStatus == L"P\u0159ipraveno",
            row.workflowStatus == L"Schv\u00E1leno");
    }

    void MainWindow::RefreshActiveFilter()
    {
        std::array<size_t, 6> counts{};
        for (auto const& row : m_rows)
        {
            for (size_t filter = 0; filter < counts.size(); ++filter)
            {
                if (agi::winui::MatchesSubtitleFilter(
                    static_cast<agi::winui::SubtitleFilter>(filter),
                    IsTranslationEmpty(row.target),
                    row.targetModified,
                    !row.qaIssue.empty(),
                    row.workflowStatus == L"P\u0159ipraveno",
                    row.workflowStatus == L"Schv\u00E1leno"))
                {
                    ++counts[filter];
                }
            }
        }

        auto setFilterText = [](ComboBoxItem const& item, wchar_t const* label, size_t count)
        {
            item.Content(box_value(hstring{ std::wstring{ label } + L" (" + std::to_wstring(count) + L")" }));
        };
        setFilterText(FilterAllItem(), L"V\u0161echny", counts[0]);
        setFilterText(FilterUntranslatedItem(), L"Nep\u0159elo\u017Een\u00E9", counts[1]);
        setFilterText(FilterModifiedItem(), L"Upraven\u00E9", counts[2]);
        setFilterText(FilterProblemsItem(), L"S probl\u00E9mem", counts[3]);
        setFilterText(FilterReadyItem(), L"P\u0159ipraven\u00E9", counts[4]);
        setFilterText(FilterApprovedItem(), L"Schv\u00E1len\u00E9", counts[5]);

        if (m_subtitleGrid && m_subtitleGrid.RowDefinitions().Size() == m_rows.size() + 1)
        {
            for (size_t index = 0; index < m_rows.size(); ++index)
            {
                bool const visible = RowMatchesActiveFilter(m_rows[index]);
                m_subtitleGrid.RowDefinitions().GetAt(static_cast<uint32_t>(index + 1)).Height(
                    GridLength{ visible ? 36.0 : 0.0, GridUnitType::Pixel });
                if (index < m_rowVisuals.size())
                {
                    for (auto const& element : m_rowVisuals[index])
                        element.Visibility(visible ? Visibility::Visible : Visibility::Collapsed);
                }
            }
        }

        bool hasPrevious = false;
        bool hasNext = false;
        for (int32_t index = 0; index < static_cast<int32_t>(m_rows.size()); ++index)
        {
            if (!RowMatchesActiveFilter(m_rows[index]))
                continue;
            hasPrevious = hasPrevious || index < m_currentIndex;
            hasNext = hasNext || index > m_currentIndex;
        }
        PreviousButton().IsEnabled(hasPrevious);
        NextButton().IsEnabled(hasNext);
    }

    void MainWindow::MoveToFilteredRow(int32_t direction)
    {
        if (m_rows.empty() || direction == 0)
            return;

        for (auto index = m_currentIndex + direction;
            index >= 0 && index < static_cast<int32_t>(m_rows.size());
            index += direction)
        {
            if (!RowMatchesActiveFilter(m_rows[index]))
                continue;
            StoreCurrentEditorSelection();
            m_currentIndex = index;
            m_selectedSubtitleIndices.assign(1, index);
            m_selectionAnchorIndex = index;
            LoadCurrentRow();
            RefreshCurrentQaVisuals();
            TargetTextBox().Focus(FocusState::Programmatic);
            return;
        }
    }

    void MainWindow::RefreshProgressSummary()
    {
        auto const untranslatedCount = std::count_if(m_rows.begin(), m_rows.end(), [this](auto const& row)
        {
            return IsTranslationEmpty(row.target);
        });
        auto const issueCount = std::count_if(m_rows.begin(), m_rows.end(), [](auto const& row)
        {
            return !row.qaIssue.empty();
        });
        auto const approvedCount = std::count_if(m_rows.begin(), m_rows.end(), [](auto const& row)
        {
            return row.workflowStatus == L"Schv\u00E1leno" && row.qaIssue.empty();
        });
        auto const reviewCount = m_rows.size() - static_cast<size_t>(approvedCount);
        auto const translatedCount = m_rows.size() - static_cast<size_t>(untranslatedCount);

        NextUntranslatedButton().Content(box_value(hstring{
            L"Dal\u0161\u00ED nep\u0159elo\u017Een\u00FD (" + std::to_wstring(untranslatedCount) + L")" }));
        NextUntranslatedButton().IsEnabled(untranslatedCount > 0);
        NextReviewButton().Content(box_value(hstring{
            L"Dal\u0161\u00ED ke kontrole (" + std::to_wstring(reviewCount) + L")" }));
        NextReviewButton().IsEnabled(reviewCount > 0);

        std::wstring summary = std::to_wstring(translatedCount) + L"/" +
            std::to_wstring(m_rows.size()) + L" p\u0159elo\u017Eeno \u00B7 " +
            std::to_wstring(approvedCount) + L" schv\u00E1leno \u00B7 probl\u00E9my " +
            std::to_wstring(issueCount);
        if (m_activeFilter != agi::winui::SubtitleFilter::all)
        {
            auto const visibleCount = std::count_if(m_rows.begin(), m_rows.end(), [this](auto const& row)
            {
                return RowMatchesActiveFilter(row);
            });
            summary += L" \u00B7 zobrazeno " + std::to_wstring(visibleCount);
        }
        if (!m_rows.empty() && m_currentIndex >= 0 && m_currentIndex < static_cast<int32_t>(m_rows.size()))
            summary += L" \u00B7 aktu\u00E1ln\u00ED #" + std::to_wstring(m_rows[m_currentIndex].number);
        TablePositionText().Text(hstring{ summary });
        RefreshActiveFilter();
        RefreshProjectOverview();
    }

    void MainWindow::RefreshApprovalAction()
    {
        if (m_rows.empty() || m_currentIndex < 0 || m_currentIndex >= static_cast<int32_t>(m_rows.size()))
        {
            ApproveButton().IsEnabled(false);
            ApproveButton().Content(box_value(hstring{ L"Schv\u00E1lit" }));
            return;
        }

        bool const approved = m_rows[m_currentIndex].workflowStatus == L"Schv\u00E1leno";
        ApproveButton().IsEnabled(true);
        ApproveButton().Content(box_value(hstring{
            approved ? L"Vr\u00E1tit ke kontrole" : L"Schv\u00E1lit" }));
        ToolTipService::SetToolTip(ApproveButton(), box_value(hstring{ approved
            ? L"Zru\u0161it schv\u00E1len\u00ED aktu\u00E1ln\u00EDho titulku"
            : L"Schv\u00E1lit a p\u0159ej\u00EDt d\u00E1l \u00B7 Ctrl+Enter" }));
    }

    bool MainWindow::RowMatchesSearch(SubtitleRowData const& row, std::wstring_view query) const
    {
        return FindOrdinalIgnoreCase(std::wstring_view{ row.original.c_str(), row.original.size() }, query) != std::wstring_view::npos ||
            FindOrdinalIgnoreCase(std::wstring_view{ row.target.c_str(), row.target.size() }, query) != std::wstring_view::npos;
    }

    void MainWindow::RefreshSearchSummary()
    {
        std::wstring const query{ SearchTextBox().Text().c_str() };
        auto const matchCount = query.empty() ? 0 : std::count_if(m_rows.begin(), m_rows.end(), [this, &query](auto const& row)
        {
            return RowMatchesActiveFilter(row) && RowMatchesSearch(row, query);
        });
        bool const hasMatches = matchCount > 0;
        SearchPreviousButton().IsEnabled(hasMatches);
        SearchNextButton().IsEnabled(hasMatches);
        SearchResultText().Text(query.empty()
            ? hstring{}
            : hstring{ std::to_wstring(matchCount) + (matchCount == 1 ? L" shoda" : L" shod") });
    }

    void MainWindow::MoveToSearchResult(int32_t direction)
    {
        std::wstring const query{ SearchTextBox().Text().c_str() };
        if (query.empty())
        {
            SearchTextBox().Focus(FocusState::Programmatic);
            return;
        }
        if (m_rows.empty() || direction == 0)
            return;

        auto const rowCount = static_cast<int32_t>(m_rows.size());
        for (int32_t offset = 1; offset <= rowCount; ++offset)
        {
            auto index = (m_currentIndex + direction * offset) % rowCount;
            if (index < 0)
                index += rowCount;
            if (!RowMatchesActiveFilter(m_rows[index]) || !RowMatchesSearch(m_rows[index], query))
                continue;

            if (index != m_currentIndex)
            {
                StoreCurrentEditorSelection();
                m_currentIndex = index;
                LoadCurrentRow();
                RefreshCurrentQaVisuals();
            }

            auto const target = std::wstring_view{ m_rows[index].target.c_str(), m_rows[index].target.size() };
            auto const targetMatch = FindOrdinalIgnoreCase(target, query);
            if (targetMatch != std::wstring_view::npos)
            {
                SetSearchEditorHighlight(
                    TargetTextBox(),
                    static_cast<int32_t>(targetMatch),
                    static_cast<int32_t>(query.size()));
            }
            TargetTextBox().Focus(FocusState::Programmatic);
            StatusBarText().Text(hstring{
                L"Hled\u00E1n\u00ED \u201E" + query + L"\u201C \u00B7 titulek #" + std::to_wstring(m_rows[index].number) });
            return;
        }

        StatusBarText().Text(hstring{ L"Hled\u00E1n\u00ED \u201E" + query + L"\u201C \u00B7 \u017E\u00E1dn\u00E1 shoda" });
        SearchTextBox().Focus(FocusState::Programmatic);
    }

    bool MainWindow::ReadSubtitleFile(
        std::wstring const& filename,
        std::vector<SubtitleEntry>& entries,
        std::wstring& errorMessage) const
    {
        auto const bridge = FindBridgeExecutable();
        if (bridge.empty())
        {
            errorMessage =
                L"Nebyl nalezen aegisub-winui-bridge.exe.\n\n"
                L"Nejprve sestavte build\\src\\aegisub-winui-bridge.exe.";
            return false;
        }

        auto output = std::filesystem::temp_directory_path();
        output /= L"srtune-read-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L".tsv";

        std::error_code fileError;
        std::filesystem::remove(output, fileError);

        std::wstring commandLine = L"\"" + bridge.wstring() + L"\" \"" + filename + L"\" \"" + output.wstring() + L"\"";
        DWORD exitCode = 0;
        if (!RunProcess(commandLine, exitCode))
        {
            errorMessage = L"Nepoda\u0159ilo se spustit aegisub-winui-bridge.exe.";
            return false;
        }

        if (!std::filesystem::exists(output))
        {
            errorMessage = L"SRTune bridge nevytvo\u0159il v\u00FDstupn\u00ED soubor. K\u00F3d: " + std::to_wstring(exitCode) + L".";
            return false;
        }

        std::ifstream stream(output, std::ios::binary);
        if (!stream)
        {
            errorMessage = L"V\u00FDstup SRTune bridge nelze otev\u0159\u00EDt.";
            std::filesystem::remove(output, fileError);
            return false;
        }

        std::string line;
        if (!std::getline(stream, line))
        {
            errorMessage = L"SRTune bridge vr\u00E1til pr\u00E1zdn\u00FD v\u00FDstup.";
            std::filesystem::remove(output, fileError);
            return false;
        }

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.rfind("ERROR\t", 0) == 0)
        {
            errorMessage = ToWide(UnescapeBridgeField(line.substr(6)));
            std::filesystem::remove(output, fileError);
            return false;
        }

        bool const protocolV1 = line == "AEGISUB-WINUI-BRIDGE\t1";
        bool const protocolV2 = line == "AEGISUB-WINUI-BRIDGE\t2";
        if (!protocolV1 && !protocolV2)
        {
            errorMessage = L"SRTune bridge vr\u00E1til nezn\u00E1m\u00FD form\u00E1t dat.";
            std::filesystem::remove(output, fileError);
            return false;
        }

        if (exitCode != 0)
        {
            errorMessage = L"SRTune bridge skon\u010Dil s chybou " + std::to_wstring(exitCode) + L".";
            std::filesystem::remove(output, fileError);
            return false;
        }

        entries.clear();
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            auto const firstTab = line.find('\t');
            auto const secondTab = firstTab == std::string::npos
                ? std::string::npos
                : line.find('\t', firstTab + 1);

            if (firstTab == std::string::npos || secondTab == std::string::npos)
            {
                continue;
            }

            auto const start = line.substr(0, firstTab);
            auto const end = line.substr(firstTab + 1, secondTab - firstTab - 1);

            std::string displayText;
            std::string rawText;
            if (protocolV2)
            {
                auto const thirdTab = line.find('\t', secondTab + 1);
                if (thirdTab == std::string::npos)
                {
                    continue;
                }
                displayText = UnescapeBridgeField(line.substr(secondTab + 1, thirdTab - secondTab - 1));
                rawText = UnescapeBridgeField(line.substr(thirdTab + 1));
            }
            else
            {
                displayText = UnescapeBridgeField(line.substr(secondTab + 1));
                rawText = displayText;
            }

            SubtitleEntry entry;
            entry.start = to_hstring(start);
            entry.end = to_hstring(end);
            entry.startSeconds = TimestampSeconds(start);
            entry.endSeconds = TimestampSeconds(end);
            entry.duration = (std::max)(0.0, entry.endSeconds - entry.startSeconds);
            entry.text = to_hstring(displayText);
            entry.rawText = to_hstring(rawText);
            entries.push_back(std::move(entry));
        }

        stream.close();
        std::filesystem::remove(output, fileError);

        if (entries.empty())
        {
            errorMessage = L"Soubor neobsahuje \u017E\u00E1dn\u00E9 dialogov\u00E9 titulky.";
            return false;
        }

        return true;
    }

    void MainWindow::BuildAlignedRows()
    {
        m_rows.clear();

        if (!m_targetEntries.empty())
        {
            m_rows.reserve(m_targetEntries.size());

            for (size_t targetIndex = 0; targetIndex < m_targetEntries.size(); ++targetIndex)
            {
                auto const& target = m_targetEntries[targetIndex];
                SubtitleRowData row;
                row.number = static_cast<int32_t>(targetIndex) + 1;
                row.start = target.start;
                row.end = target.end;
                row.duration = target.duration;
                row.target = target.text;
                row.rawTarget = target.rawText;
                row.savedTarget = row.target;
                row.savedStart = row.start;
                row.savedEnd = row.end;
                row.timingModified = false;
                row.status = L"P\u0159ipraveno";
                row.targetModified = false;
                row.historyInitialized = true;

                std::vector<SubtitleEntry const*> matchedSources;
                SubtitleEntry const* bestOverlapSource = nullptr;
                SubtitleEntry const* nearestSource = nullptr;
                double bestQuality = 0.0;
                double nearestCenterDistance = (std::numeric_limits<double>::max)();
                double const targetCenter = (target.startSeconds + target.endSeconds) / 2.0;
                for (auto const& source : m_sourceEntries)
                {
                    double const quality = agi::winui::SubtitleOverlapQuality(
                        target.startSeconds, target.endSeconds, source.startSeconds, source.endSeconds);
                    if (agi::winui::ShouldPairSubtitles(quality))
                        matchedSources.push_back(&source);
                    if (quality > bestQuality)
                    {
                        bestQuality = quality;
                        bestOverlapSource = &source;
                    }
                    double const sourceCenter = (source.startSeconds + source.endSeconds) / 2.0;
                    double const centerDistance = std::abs(targetCenter - sourceCenter);
                    if (centerDistance < nearestCenterDistance)
                    {
                        nearestCenterDistance = centerDistance;
                        nearestSource = &source;
                    }
                }

                if (matchedSources.empty())
                {
                    if (bestOverlapSource)
                        matchedSources.push_back(bestOverlapSource);
                    else if (nearestSource && nearestCenterDistance <= 0.75)
                    {
                        matchedSources.push_back(nearestSource);
                        bestQuality = 0.01;
                    }
                }

                std::wstring original;
                for (auto const* source : matchedSources)
                {
                    if (!original.empty())
                        original += L"\n";
                    original += source->text.c_str();
                    if (row.sourceStart.empty())
                        row.sourceStart = source->start;
                    row.sourceEnd = source->end;
                }
                row.original = hstring{ original };
                row.sourceMatchQuality = bestQuality;
                if (matchedSources.empty())
                {
                    row.sourceStart = L"";
                    row.sourceEnd = L"";
                }

                m_rows.push_back(std::move(row));
            }
        }
        else
        {
            m_rows.reserve(m_sourceEntries.size());
            for (size_t sourceIndex = 0; sourceIndex < m_sourceEntries.size(); ++sourceIndex)
            {
                auto const& source = m_sourceEntries[sourceIndex];
                SubtitleRowData row;
                row.number = static_cast<int32_t>(sourceIndex) + 1;
                row.start = source.start;
                row.end = source.end;
                row.duration = source.duration;
                row.sourceStart = source.start;
                row.sourceEnd = source.end;
                row.original = source.text;
                row.target = L"";
                row.rawTarget = L"";
                row.savedTarget = row.target;
                row.savedStart = row.start;
                row.savedEnd = row.end;
                row.timingModified = false;
                row.status = L"P\u0159ipraveno";
                row.targetModified = false;
                row.historyInitialized = true;
                m_rows.push_back(std::move(row));
            }
        }

        m_currentIndex = 0;
    }

    bool MainWindow::SaveTargetSubtitleFile(std::wstring& errorMessage, std::wstring const& destinationPath)
    {
        m_lastSaveCreatedBackup = false;
        m_lastSaveDetectedExternalChange = false;
        auto const previousDraftPath = WorkspaceDraftPath(m_targetPath);
        auto const savePath = destinationPath.empty() ? std::wstring{ m_targetPath.c_str() } : destinationPath;
        auto const templatePath = m_targetPath.empty()
            ? std::wstring{ m_sourcePath.c_str() }
            : std::wstring{ m_targetPath.c_str() };
        if (savePath.empty() || m_rows.empty())
        {
            errorMessage = L"Projekt zatím neobsahuje žádné titulky k uložení.";
            return false;
        }

        auto const targetPath = std::filesystem::path(savePath);

        // A completely new project has no subtitle file to use as a formatting
        // template. In that case write a clean UTF-8 SubRip file directly.
        if (templatePath.empty())
        {
            auto extension = targetPath.extension().wstring();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
            if (extension != L".srt")
            {
                errorMessage = L"Nový projekt bez zdrojového souboru lze zatím uložit jako SubRip (*.srt).";
                return false;
            }

            std::ofstream stream(targetPath, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                errorMessage = L"Výstupní SRT soubor nelze vytvořit.";
                return false;
            }

            stream.write("\xEF\xBB\xBF", 3);

            auto srtTimestamp = [](winrt::hstring const& value)
            {
                auto result = winrt::to_string(value);
                auto const separator = result.rfind('.');
                if (separator != std::string::npos)
                    result[separator] = ',';
                return result;
            };

            for (size_t index = 0; index < m_rows.size(); ++index)
            {
                auto& row = m_rows[index];
                stream << (index + 1) << "\r\n"
                       << srtTimestamp(row.start) << " --> " << srtTimestamp(row.end) << "\r\n"
                       << winrt::to_string(row.target) << "\r\n\r\n";
            }

            if (!stream)
            {
                errorMessage = L"SRT soubor se nepodařilo celý zapsat.";
                return false;
            }
            stream.close();

            m_targetEntries.clear();
            m_targetEntries.reserve(m_rows.size());
            for (auto& row : m_rows)
            {
                row.rawTarget = row.target;
                row.savedTarget = row.target;
                row.savedStart = row.start;
                row.savedEnd = row.end;
                row.savedWorkflowStatus = row.workflowStatus;
                row.historyInitialized = true;
                row.targetModified = false;
                row.timingModified = false;
                row.editSequenceKind = 0;

                SubtitleEntry entry;
                entry.start = row.start;
                entry.end = row.end;
                entry.startSeconds = TimestampSeconds(winrt::to_string(row.start));
                entry.endSeconds = TimestampSeconds(winrt::to_string(row.end));
                entry.duration = (std::max)(0.0, entry.endSeconds - entry.startSeconds);
                entry.text = row.target;
                entry.rawText = row.target;
                m_targetEntries.push_back(std::move(entry));
            }

            m_targetPath = winrt::hstring{ savePath };
            m_structureDirty = false;
            m_forceSaveAsForRecoveredDraft = false;
            m_hasTargetFileFingerprint = FileFingerprint(
                targetPath, m_targetFileSize, m_targetFileTimestamp);
            m_externalChangeAcknowledged = false;
            m_workflowStateDirty = false;
            RefreshProjectFileLabels();
            SaveWorkspaceState();
            DeleteWorkspaceDraft();
            SaveRecentProjectPaths();
            RefreshRecentProjectAction();
            SetDirty(false);
            return true;
        }

        if (PathsReferToSameFile(
            std::wstring_view{ m_sourcePath.c_str(), m_sourcePath.size() }, savePath))
        {
            errorMessage = L"P\u0159eklad nelze ulo\u017Eit p\u0159es soubor origin\u00E1lu. Zvolte jin\u00FD n\u00E1zev souboru.";
            return false;
        }

        if ((m_hasTargetFileFingerprint || m_forceSaveAsForRecoveredDraft) && !m_targetPath.empty() &&
            PathsReferToSameFile(
                std::wstring_view{ m_targetPath.c_str(), m_targetPath.size() }, savePath))
        {
            uintmax_t currentSize{};
            int64_t currentTimestamp{};
            if (m_forceSaveAsForRecoveredDraft ||
                !FileFingerprint(targetPath, currentSize, currentTimestamp) ||
                currentSize != m_targetFileSize || currentTimestamp != m_targetFileTimestamp)
            {
                m_lastSaveDetectedExternalChange = true;
                errorMessage = m_forceSaveAsForRecoveredDraft
                    ? L"Obnoven\u00FD koncept poch\u00E1z\u00ED ze star\u0161\u00ED verze souboru p\u0159ekladu. SRTune jej proto nep\u0159epsal."
                    : L"Otev\u0159en\u00FD soubor p\u0159ekladu od posledn\u00EDho na\u010Dten\u00ED zm\u011Bnila jin\u00E1 aplikace. SRTune jej proto nep\u0159epsal.";
                return false;
            }
        }

        if (!m_targetEntries.empty() && m_rows.size() != m_targetEntries.size())
        {
            errorMessage = L"Po\u010Det pracovn\u00EDch \u0159\u00E1dk\u016F neodpov\u00EDd\u00E1 souboru p\u0159ekladu.";
            return false;
        }

        auto const bridge = FindBridgeExecutable();
        if (bridge.empty())
        {
            errorMessage = L"Nebyl nalezen aegisub-winui-bridge.exe.";
            return false;
        }

        auto updateFile = std::filesystem::temp_directory_path();
        updateFile /= L"srtune-write-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L".tsv";

        auto tempOutput = targetPath.parent_path();
        tempOutput /= targetPath.filename().wstring() +
            L".winui-" + std::to_wstring(GetCurrentProcessId()) +
            L"-" + std::to_wstring(GetTickCount64()) + L".tmp";

        std::error_code fileError;
        std::filesystem::remove(updateFile, fileError);
        std::filesystem::remove(tempOutput, fileError);

        {
            std::ofstream stream(updateFile, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                errorMessage = L"Nelze vytvo\u0159it do\u010Dasn\u00FD soubor pro ulo\u017Een\u00ED titulk\u016F.";
                return false;
            }

            stream << "AEGISUB-WINUI-BRIDGE\t3\n";
            for (auto const& row : m_rows)
            {
                auto const textForSave = row.targetModified ? row.target : row.rawTarget;
                stream << to_string(row.start) << '\t'
                       << to_string(row.end) << '\t'
                       << EscapeBridgeField(to_string(textForSave)) << '\n';
            }
        }

        std::wstring commandLine = L"\"" + bridge.wstring() + L"\" --write \"" +
            templatePath + L"\" \"" + updateFile.wstring() + L"\" \"" + tempOutput.wstring() + L"\"";

        DWORD exitCode = 0;
        if (!RunProcess(commandLine, exitCode))
        {
            std::filesystem::remove(updateFile, fileError);
            errorMessage = L"Nepoda\u0159ilo se spustit SRTune bridge pro ulo\u017Een\u00ED.";
            return false;
        }

        std::filesystem::remove(updateFile, fileError);

        if (exitCode != 0)
        {
            if (!ReadBridgeError(tempOutput, errorMessage))
            {
                errorMessage = L"SRTune bridge skon\u010Dil p\u0159i ukl\u00E1d\u00E1n\u00ED s chybou " + std::to_wstring(exitCode) + L".";
            }
            std::filesystem::remove(tempOutput, fileError);
            return false;
        }

        if (!std::filesystem::exists(tempOutput))
        {
            errorMessage = L"SRTune bridge nevytvo\u0159il ulo\u017Een\u00FD soubor.";
            return false;
        }

        if (std::filesystem::exists(targetPath))
        {
            auto const backupPath = WorkspaceBackupPath(targetPath);
            if (backupPath.empty())
            {
                std::filesystem::remove(tempOutput, fileError);
                errorMessage = L"Nepoda\u0159ilo se zjistit syst\u00E9mov\u00FD adres\u00E1\u0159 pro bezpe\u010Dnostn\u00ED z\u00E1lohu. "
                    L"P\u016Fvodn\u00ED soubor p\u0159ekladu nebyl zm\u011Bn\u011Bn.";
                return false;
            }
            fileError.clear();
            std::filesystem::create_directories(backupPath.parent_path(), fileError);
            if (fileError)
            {
                std::filesystem::remove(tempOutput, fileError);
                errorMessage = L"Nepoda\u0159ilo se p\u0159ipravit syst\u00E9mov\u00FD adres\u00E1\u0159 pro bezpe\u010Dnostn\u00ED z\u00E1lohu. "
                    L"P\u016Fvodn\u00ED soubor p\u0159ekladu nebyl zm\u011Bn\u011Bn.";
                return false;
            }
            auto backupTemp = backupPath;
            backupTemp += L".tmp-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(GetTickCount64());
            std::filesystem::remove(backupTemp, fileError);

            if (!CopyFileW(targetPath.c_str(), backupTemp.c_str(), FALSE) ||
                !MoveFileExW(backupTemp.c_str(), backupPath.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                std::filesystem::remove(backupTemp, fileError);
                std::filesystem::remove(tempOutput, fileError);
                errorMessage = L"Nepoda\u0159ilo se vytvo\u0159it bezpe\u010Dnostn\u00ED z\u00E1lohu v LocalAppData. "
                    L"P\u016Fvodn\u00ED soubor p\u0159ekladu nebyl zm\u011Bn\u011Bn.";
                return false;
            }
            m_lastSaveCreatedBackup = true;
            CleanupWorkspaceBackups(backupPath.parent_path(), backupPath);
        }

        if (!MoveFileExW(
            tempOutput.c_str(),
            targetPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            errorMessage = L"Do\u010Dasn\u00FD soubor se poda\u0159ilo vytvo\u0159it, ale nepoda\u0159ilo se nahradit p\u016Fvodn\u00ED soubor p\u0159ekladu.";
            std::filesystem::remove(tempOutput, fileError);
            return false;
        }

        m_targetEntries.clear();
        m_targetEntries.reserve(m_rows.size());
        for (auto& row : m_rows)
        {
            auto savedRaw = row.targetModified ? row.target : row.rawTarget;
            row.rawTarget = savedRaw;
            row.savedTarget = row.target;
            row.savedStart = row.start;
            row.savedEnd = row.end;
            row.savedWorkflowStatus = row.workflowStatus;
            row.historyInitialized = true;
            row.targetModified = false;
            row.timingModified = false;
            row.editSequenceKind = 0;

            SubtitleEntry entry;
            entry.start = row.start;
            entry.end = row.end;
            entry.startSeconds = TimestampSeconds(to_string(row.start));
            entry.endSeconds = TimestampSeconds(to_string(row.end));
            entry.duration = (std::max)(0.0, entry.endSeconds - entry.startSeconds);
            entry.text = row.target;
            entry.rawText = savedRaw;
            m_targetEntries.push_back(std::move(entry));
        }
        m_structureDirty = false;

        if (m_workspaceDraftTimer)
            m_workspaceDraftTimer.Stop();
        std::filesystem::remove(previousDraftPath, fileError);
        m_targetPath = hstring{ savePath };
        m_forceSaveAsForRecoveredDraft = false;
        m_hasTargetFileFingerprint = FileFingerprint(
            targetPath, m_targetFileSize, m_targetFileTimestamp);
        RefreshProjectFileLabels();
        SaveWorkspaceState();
        DeleteWorkspaceDraft();
        SaveRecentProjectPaths();
        RefreshRecentProjectAction();
        m_externalChangeAcknowledged = false;
        m_workflowStateDirty = false;
        SetDirty(false);

        return true;
    }
}
