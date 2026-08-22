#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
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
            L"Origin\u00E1l a \u010Desk\u00FD p\u0159eklad mus\u00ED b\u00FDt dva r\u016Fzn\u00E9 soubory.\n\n"
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
            L"Aegisub" / L"TranslationWorkspace" / filename.str();
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
            L"Aegisub" / L"TranslationWorkspace" / L"last-project.tsv";
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
            directory.parent_path().filename() != L"Aegisub")
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

namespace winrt::Aegisub_WinUI::implementation
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
        InitializeDynamicSubtitleGrid();
        RebuildSubtitleGrid();
        HookWindowClosing();
        StartExternalChangeMonitoring();
        RefreshRecentProjectAction();
        LoadCurrentRow();
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
            MessageBoxW(GetActiveWindow(), L"Pro aktu\u00E1ln\u00ED \u010Desk\u00FD soubor nebyla nalezena z\u00E1loha.",
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
            (L"aegisub-winui-restore-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
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
            MessageBoxW(GetActiveWindow(), L"Nejd\u0159\u00EDve otev\u0159ete \u010Desk\u00FD soubor titulk\u016F.",
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
                L"Odstranit z\u00E1lohu a pracovn\u00ED koncept aktu\u00E1ln\u00EDho \u010Desk\u00E9ho souboru?\n\n"
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
            ? L"Ve zobrazen\u00FDch \u010Desk\u00FDch titulc\u00EDch nebyla nalezena \u017E\u00E1dn\u00E1 shoda."
            : L"Nalezeno " + std::to_wstring(occurrenceCount) + L" v\u00FDskyt\u016F v " +
                std::to_wstring(rowCount) + L" titulc\u00EDch." + details;
        MessageBoxW(GetActiveWindow(), message.c_str(), L"N\u00E1hled nahrazen\u00ED",
            MB_OK | (occurrenceCount == 0 ? MB_ICONINFORMATION : MB_ICONASTERISK));
    }

    void MainWindow::CaptureBulkSnapshot(std::vector<size_t> const& indices, hstring const& action)
    {
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
            {
                row.undoHistory.push_back(row.target);
                if (row.undoHistory.size() > 200)
                {
                    row.undoHistory.erase(row.undoHistory.begin());
                }
            }
            row.redoHistory.clear();
            row.editSequenceKind = edit.kind;
            row.editSequencePosition = edit.position + (edit.kind == 1 ? 1 : 0);
        }
        else if (applyingHistory)
        {
            row.editSequenceKind = 0;
        }

        row.target = newText;
        row.targetModified = !agi::winui::EquivalentEditorText(row.target.c_str(), row.savedTarget.c_str());
        row.status = row.targetModified
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
        RefreshSearchSummary();
        RefreshProgressSummary();
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

        std::wstring targetInfo = L"#" + std::to_wstring(row.number) + L" \u00B7 ";
        targetInfo += row.status.c_str();
        TargetInfoText().Text(hstring{ targetInfo });
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

        TranscriptCurrentTimeText().Text(row.start);
        TranscriptCurrentText().Text(row.original);

        if (m_currentIndex > 0)
        {
            auto const& previous = m_rows[m_currentIndex - 1];
            TranscriptPreviousBlock().Visibility(Visibility::Visible);
            TranscriptPreviousTimeText().Text(previous.start);
            TranscriptPreviousText().Text(previous.original);
        }
        else
        {
            TranscriptPreviousBlock().Visibility(Visibility::Collapsed);
        }

        if (m_currentIndex < static_cast<int32_t>(m_rows.size()) - 1)
        {
            auto const& next = m_rows[m_currentIndex + 1];
            TranscriptNextBlock().Visibility(Visibility::Visible);
            TranscriptNextTimeText().Text(next.start);
            TranscriptNextText().Text(next.original);
        }
        else
        {
            TranscriptNextBlock().Visibility(Visibility::Collapsed);
        }

        RefreshProgressSummary();

        UpdateSelectionVisuals();
        ScrollCurrentRowIntoView();
        UpdateMetrics();

        if (m_sourcePath.empty())
        {
            StatusBarText().Text(L"Uk\u00E1zkov\u00E1 data \u00B7 tla\u010D\u00EDtko Otev\u0159\u00EDt projekt na\u010Dte origin\u00E1l a \u010De\u0161tinu");
        }
        else if (m_targetPath.empty())
        {
            auto const sourceName = std::filesystem::path(m_sourcePath.c_str()).filename().wstring();
            StatusBarText().Text(hstring{ L"Origin\u00E1l: " + sourceName + L" \u00B7 \u010De\u0161tina nen\u00ED na\u010Dtena" });
        }
        else
        {
            auto const sourceName = std::filesystem::path(m_sourcePath.c_str()).filename().wstring();
            auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
            StatusBarText().Text(hstring{
                L"Origin\u00E1l: " + sourceName + L" \u00B7 \u010Ce\u0161tina: " + targetName + L" \u00B7 p\u00E1rov\u00E1n\u00ED podle \u010Dasu" });
        }

        m_loadingSelection = false;
        ScheduleWorkspaceDraftSave();
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

        for (auto const character : text)
        {
            if (character == L'\r')
            {
                continue;
            }

            if (character == L'\n')
            {
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

    void MainWindow::UpdateSelectionVisuals()
    {
        auto const accentBrush = TargetPanelBorder().BorderBrush();

        for (auto const& border : m_rowBorders)
        {
            border.BorderThickness(Thickness{ 0.0, 0.0, 0.0, 0.0 });
            border.BorderBrush(accentBrush);
        }

        if (m_currentIndex >= 0 && m_currentIndex < static_cast<int32_t>(m_rowBorders.size()))
        {
            m_rowBorders[m_currentIndex].BorderThickness(Thickness{ 4.0, 1.0, 0.0, 1.0 });
        }
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

        m_rowBorders[m_currentIndex].StartBringIntoView();
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
        for (auto const& child : RootGrid().Children())
        {
            auto const outerBorder = child.try_as<Border>();
            if (!outerBorder || Grid::GetRow(outerBorder) != 2)
            {
                continue;
            }

            auto const container = outerBorder.Child().try_as<Grid>();
            if (!container)
            {
                continue;
            }

            for (auto const& containerChild : container.Children())
            {
                auto const scrollViewer = containerChild.try_as<ScrollViewer>();
                if (!scrollViewer || Grid::GetRow(scrollViewer) != 1)
                {
                    continue;
                }

                m_subtitleGrid = scrollViewer.Content().try_as<Grid>();
                if (m_subtitleGrid)
                {
                    return;
                }
            }
        }
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
        m_rowTargetTexts.clear();
        m_rowStatusTexts.clear();
        m_rowVisuals.clear();
        m_rowVisuals.resize(m_rows.size());

        RowDefinition headerRow;
        headerRow.Height(GridLength{ 34.0, GridUnitType::Pixel });
        grid.RowDefinitions().Append(headerRow);

        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            RowDefinition rowDefinition;
            rowDefinition.Height(GridLength{ 36.0, GridUnitType::Pixel });
            grid.RowDefinitions().Append(rowDefinition);
        }

        auto const accentBrush = TargetPanelBorder().BorderBrush();

        Border headerBorder;
        headerBorder.BorderBrush(accentBrush);
        headerBorder.BorderThickness(Thickness{ 0.0, 1.0, 0.0, 1.0 });
        Grid::SetRow(headerBorder, 0);
        Grid::SetColumnSpan(headerBorder, 6);
        grid.Children().Append(headerBorder);

        auto addText = [&](hstring const& text, int32_t row, int32_t column, bool ellipsis, double leftMargin)
        {
            TextBlock block;
            block.Text(text);
            block.Margin(Thickness{ leftMargin, 0.0, 4.0, 0.0 });
            block.VerticalAlignment(VerticalAlignment::Center);
            block.IsHitTestVisible(false);
            if (ellipsis)
            {
                block.TextTrimming(TextTrimming::CharacterEllipsis);
            }
            Grid::SetRow(block, row);
            Grid::SetColumn(block, column);
            grid.Children().Append(block);
            return block;
        };

        addText(L"#", 0, 0, false, 10.0);
        addText(L"Start", 0, 1, false, 8.0);
        addText(L"Konec", 0, 2, false, 8.0);
        addText(L"Origin\u00E1l", 0, 3, false, 8.0);
        addText(L"\u010Ce\u0161tina", 0, 4, false, 8.0);
        addText(L"Stav", 0, 5, false, 8.0);

        for (int32_t index = 0; index < static_cast<int32_t>(m_rows.size()); ++index)
        {
            auto const& row = m_rows[index];
            auto const visualRow = index + 1;

            Microsoft::UI::Xaml::Media::SolidColorBrush transparentBrush;
            transparentBrush.Color(Windows::UI::Color{ 0, 0, 0, 0 });

            Border rowBorder;
            rowBorder.Background(transparentBrush);
            rowBorder.BorderBrush(accentBrush);
            Grid::SetRow(rowBorder, visualRow);
            Grid::SetColumnSpan(rowBorder, 6);
            rowBorder.Tapped([this, index](auto const&, auto const&)
            {
                if (index < 0 || index >= static_cast<int32_t>(m_rows.size()) || index == m_currentIndex)
                {
                    return;
                }

                StoreCurrentEditorSelection();
                m_currentIndex = index;
                LoadCurrentRow();
                TargetTextBox().Focus(FocusState::Programmatic);
            });
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
            visuals.push_back(numberText.as<UIElement>());
            visuals.push_back(startText.as<UIElement>());
            visuals.push_back(endText.as<UIElement>());
            visuals.push_back(originalText.as<UIElement>());
            visuals.push_back(targetText.as<UIElement>());
            visuals.push_back(statusText.as<UIElement>());
            m_rowTargetTexts.push_back(targetText);
            m_rowStatusTexts.push_back(statusText);
        }

        RefreshActiveFilter();
        UpdateSelectionVisuals();
    }

    void MainWindow::SetDirty(bool dirty)
    {
        m_hasUnsavedChanges = dirty;
        Title(dirty
            ? L"Aegisub Translation Workspace *"
            : L"Aegisub Translation Workspace");
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
            L"Aegisub Translation Workspace",
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
            L"Otev\u0159en\u00FD \u010Desk\u00FD soubor zm\u011Bnila jin\u00E1 aplikace.\n\n"
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
            StatusBarText().Text(L"Extern\u011B zm\u011Bn\u011Bn\u00FD \u010Desk\u00FD soubor byl znovu na\u010Dten");
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
        RefreshLoadedProject();
    }

    bool MainWindow::SelectSubtitleFile(std::wstring const& title, std::wstring& filename) const
    {
        wchar_t buffer[32768]{};
        wchar_t const filter[] =
            L"Titulky (*.srt;*.ass;*.ssa)\0*.srt;*.ass;*.ssa\0"
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
            L"Titulky Advanced SubStation Alpha (*.ass)\0*.ass\0"
            L"Titulky SubStation Alpha (*.ssa)\0*.ssa\0"
            L"V\u0161echny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Ulo\u017Eit \u010Desk\u00E9 titulky jako";
        dialog.lpstrDefExt = L"srt";
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

        if (!GetSaveFileNameW(&dialog))
            return false;

        filename = buffer;
        return true;
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
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        std::wstring targetFilename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt p\u0159ipraven\u00E9 \u010Desk\u00E9 titulky", targetFilename))
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
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        m_sourceEntries = std::move(sourceEntries);
        m_sourcePath = hstring{ sourceFilename };
        m_targetEntries = std::move(targetEntries);
        m_targetPath = hstring{ targetFilename };
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
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        std::vector<SubtitleEntry> refreshedTargetEntries;
        if (!m_targetPath.empty()
            && !ReadSubtitleFile(m_targetPath.c_str(), refreshedTargetEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
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
        if (!ConfirmSaveBefore(L"otev\u0159en\u00EDm jin\u00E9ho \u010Desk\u00E9ho p\u0159ekladu"))
            return;

        std::wstring filename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt p\u0159ipraven\u00E9 \u010Desk\u00E9 titulky", filename))
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
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        m_targetEntries = std::move(entries);
        m_targetPath = hstring{ filename };
        RefreshLoadedProject();
    }

    void MainWindow::RefreshLoadedProject()
    {
        m_externalChangeAcknowledged = false;
        ClearBulkUndo();
        BuildAlignedRows();
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
                L" titulk\u016F \u00B7 \u010De\u0161tina " + std::to_wstring(m_targetEntries.size()) +
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

        update(OriginalFileText(), m_sourcePath, L"Soubor nen\u00ED na\u010Dten");
        update(TargetFileText(), m_targetPath, L"Nov\u00FD p\u0159eklad \u00B7 zat\u00EDm neulo\u017Een");
        RefreshBackupAction();
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
                L"Pro otev\u0159en\u00FD \u010Desk\u00FD soubor zat\u00EDm nen\u00ED dostupn\u00E1 z\u00E1loha"));
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
                TargetTextBox().SelectionStart(static_cast<int32_t>(targetMatch));
                TargetTextBox().SelectionLength(static_cast<int32_t>(query.size()));
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
        output /= L"aegisub-winui-read-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L".tsv";

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
            errorMessage = L"Aegisub bridge nevytvo\u0159il v\u00FDstupn\u00ED soubor. K\u00F3d: " + std::to_wstring(exitCode) + L".";
            return false;
        }

        std::ifstream stream(output, std::ios::binary);
        if (!stream)
        {
            errorMessage = L"V\u00FDstup Aegisub bridge nelze otev\u0159\u00EDt.";
            std::filesystem::remove(output, fileError);
            return false;
        }

        std::string line;
        if (!std::getline(stream, line))
        {
            errorMessage = L"Aegisub bridge vr\u00E1til pr\u00E1zdn\u00FD v\u00FDstup.";
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
            errorMessage = L"Aegisub bridge vr\u00E1til nezn\u00E1m\u00FD form\u00E1t dat.";
            std::filesystem::remove(output, fileError);
            return false;
        }

        if (exitCode != 0)
        {
            errorMessage = L"Aegisub bridge skon\u010Dil s chybou " + std::to_wstring(exitCode) + L".";
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
        if (savePath.empty() || templatePath.empty() || m_rows.empty())
        {
            errorMessage = L"Nejd\u0159\u00EDve na\u010Dt\u011Bte origin\u00E1ln\u00ED nebo p\u0159ipraven\u00E9 \u010Desk\u00E9 titulky.";
            return false;
        }

        auto const targetPath = std::filesystem::path(savePath);

        if (PathsReferToSameFile(
            std::wstring_view{ m_sourcePath.c_str(), m_sourcePath.size() }, savePath))
        {
            errorMessage = L"\u010Cesk\u00FD p\u0159eklad nelze ulo\u017Eit p\u0159es soubor origin\u00E1lu. Zvolte jin\u00FD n\u00E1zev souboru.";
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
                    ? L"Obnoven\u00FD koncept poch\u00E1z\u00ED ze star\u0161\u00ED verze \u010Desk\u00E9ho souboru. Aegisub jej proto nep\u0159epsal."
                    : L"Otev\u0159en\u00FD \u010Desk\u00FD soubor od posledn\u00EDho na\u010Dten\u00ED zm\u011Bnila jin\u00E1 aplikace. Aegisub jej proto nep\u0159epsal.";
                return false;
            }
        }

        if (!m_targetEntries.empty() && m_rows.size() != m_targetEntries.size())
        {
            errorMessage = L"Po\u010Det pracovn\u00EDch \u0159\u00E1dk\u016F neodpov\u00EDd\u00E1 \u010Desk\u00E9mu souboru.";
            return false;
        }

        auto const bridge = FindBridgeExecutable();
        if (bridge.empty())
        {
            errorMessage = L"Nebyl nalezen aegisub-winui-bridge.exe.";
            return false;
        }

        auto updateFile = std::filesystem::temp_directory_path();
        updateFile /= L"aegisub-winui-write-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L".tsv";

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

            stream << "AEGISUB-WINUI-BRIDGE\t1\n";
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
            errorMessage = L"Nepoda\u0159ilo se spustit Aegisub bridge pro ulo\u017Een\u00ED.";
            return false;
        }

        std::filesystem::remove(updateFile, fileError);

        if (exitCode != 0)
        {
            if (!ReadBridgeError(tempOutput, errorMessage))
            {
                errorMessage = L"Aegisub bridge skon\u010Dil p\u0159i ukl\u00E1d\u00E1n\u00ED s chybou " + std::to_wstring(exitCode) + L".";
            }
            std::filesystem::remove(tempOutput, fileError);
            return false;
        }

        if (!std::filesystem::exists(tempOutput))
        {
            errorMessage = L"Aegisub bridge nevytvo\u0159il ulo\u017Een\u00FD soubor.";
            return false;
        }

        if (std::filesystem::exists(targetPath))
        {
            auto const backupPath = WorkspaceBackupPath(targetPath);
            if (backupPath.empty())
            {
                std::filesystem::remove(tempOutput, fileError);
                errorMessage = L"Nepoda\u0159ilo se zjistit syst\u00E9mov\u00FD adres\u00E1\u0159 pro bezpe\u010Dnostn\u00ED z\u00E1lohu. "
                    L"P\u016Fvodn\u00ED \u010Desk\u00FD soubor nebyl zm\u011Bn\u011Bn.";
                return false;
            }
            fileError.clear();
            std::filesystem::create_directories(backupPath.parent_path(), fileError);
            if (fileError)
            {
                std::filesystem::remove(tempOutput, fileError);
                errorMessage = L"Nepoda\u0159ilo se p\u0159ipravit syst\u00E9mov\u00FD adres\u00E1\u0159 pro bezpe\u010Dnostn\u00ED z\u00E1lohu. "
                    L"P\u016Fvodn\u00ED \u010Desk\u00FD soubor nebyl zm\u011Bn\u011Bn.";
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
                    L"P\u016Fvodn\u00ED \u010Desk\u00FD soubor nebyl zm\u011Bn\u011Bn.";
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
            errorMessage = L"Do\u010Dasn\u00FD soubor se poda\u0159ilo vytvo\u0159it, ale nepoda\u0159ilo se nahradit p\u016Fvodn\u00ED \u010Desk\u00FD soubor.";
            std::filesystem::remove(tempOutput, fileError);
            return false;
        }

        if (m_targetEntries.empty())
        {
            m_targetEntries = m_sourceEntries;
        }
        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            auto savedRaw = m_rows[i].targetModified ? m_rows[i].target : m_rows[i].rawTarget;
            m_rows[i].rawTarget = savedRaw;
            m_rows[i].savedTarget = m_rows[i].target;
            m_rows[i].savedWorkflowStatus = m_rows[i].workflowStatus;
            m_rows[i].historyInitialized = true;
            m_rows[i].targetModified = false;
            m_rows[i].editSequenceKind = 0;
            m_targetEntries[i].start = m_rows[i].start;
            m_targetEntries[i].end = m_rows[i].end;
            m_targetEntries[i].text = m_rows[i].target;
            m_targetEntries[i].rawText = savedRaw;
        }

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
