#pragma once

#include "MainWindow.g.h"
#include "../../../src/winui_bridge_text.h"

#include <algorithm>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <winrt/Windows.System.h>

namespace winrt::Aegisub_WinUI::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow()
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void RootGrid_Loaded(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void PreviousButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void NextButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void ApproveButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void SaveButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void SaveAsButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void RestoreBackupButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OpenBothButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OpenSourceButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OpenTargetButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OpenRecentProjectButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void RecoveryOverviewButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OpenRecoveryFolderButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void DeleteRecoveryFilesButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void TargetTextBox_TextChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);

        void SubtitleRow_Tapped(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& args);

        void TargetTextBox_WorkflowLoaded(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void InsertLineBreakButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void CopyOriginalButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void NextProblemButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void NextUntranslatedButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void NextReviewButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void SearchTextBox_TextChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);

        void SearchTextBox_KeyDown(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);

        void SearchPreviousButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void SearchNextButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void FilterComboBox_SelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

    private:
        struct SubtitleEntry
        {
            winrt::hstring start;
            winrt::hstring end;
            double startSeconds{};
            double endSeconds{};
            double duration{};
            winrt::hstring text;
            winrt::hstring rawText;
        };

        struct SubtitleRowData
        {
            int32_t number{};
            winrt::hstring start;
            winrt::hstring end;
            double duration{};
            winrt::hstring sourceStart;
            winrt::hstring sourceEnd;
            winrt::hstring original;
            winrt::hstring target;
            winrt::hstring rawTarget;
            winrt::hstring status;
            bool targetModified{};
            winrt::hstring workflowStatus;
            winrt::hstring qaIssue;
            double sourceMatchQuality{};
            winrt::hstring savedTarget;
            winrt::hstring savedWorkflowStatus;
            std::vector<winrt::hstring> undoHistory;
            std::vector<winrt::hstring> redoHistory;
            bool historyInitialized{};
            int editSequenceKind{};
            size_t editSequencePosition{};
            int32_t selectionStart{};
            int32_t selectionLength{};
            bool selectionInitialized{};
        };

        std::vector<SubtitleRowData> m_rows
        {
            { 143, L"00:12:38.310", L"00:12:41.420", 3.11, L"00:12:38.310", L"00:12:41.420", L"We still have time.", L"Po\u0159\u00E1d m\u00E1me \u010Das.", L"Po\u0159\u00E1d m\u00E1me \u010Das.", L"Schv\u00E1leno", false },
            { 144, L"00:12:41.980", L"00:12:43.980", 2.00, L"00:12:41.980", L"00:12:43.980", L"If the road stays clear, we can still make it.", L"Jestli z\u016Fstane cesta voln\u00E1, po\u0159\u00E1d to stihneme.", L"Jestli z\u016Fstane cesta voln\u00E1, po\u0159\u00E1d to stihneme.", L"Schv\u00E1leno", false },
            { 145, L"00:12:44.120", L"00:12:46.840", 2.72, L"00:12:44.120", L"00:12:46.840", L"We should be there before sunrise.", L"M\u011Bli bychom tam b\u00FDt p\u0159ed v\u00FDchodem slunce.", L"M\u011Bli bychom tam b\u00FDt p\u0159ed v\u00FDchodem slunce.", L"Upraveno", false },
            { 146, L"00:12:47.050", L"00:12:49.300", 2.25, L"00:12:47.050", L"00:12:49.300", L"Then we wait for the signal.", L"Pak po\u010Dk\u00E1me na sign\u00E1l.", L"Pak po\u010Dk\u00E1me na sign\u00E1l.", L"P\u0159ipraveno", false },
            { 147, L"00:12:50.100", L"00:12:52.650", 2.55, L"00:12:50.100", L"00:12:52.650", L"No mistakes this time.", L"Tentokr\u00E1t bez chyb.", L"Tentokr\u00E1t bez chyb.", L"P\u0159ipraveno", false },
        };

        std::vector<SubtitleEntry> m_sourceEntries;
        std::vector<SubtitleEntry> m_targetEntries;
        std::vector<winrt::Microsoft::UI::Xaml::Controls::Border> m_rowBorders;
        std::vector<winrt::Microsoft::UI::Xaml::Controls::TextBlock> m_rowTargetTexts;
        std::vector<winrt::Microsoft::UI::Xaml::Controls::TextBlock> m_rowStatusTexts;
        winrt::Microsoft::UI::Xaml::Controls::Grid m_subtitleGrid{ nullptr };
        std::vector<std::vector<winrt::Microsoft::UI::Xaml::UIElement>> m_rowVisuals;
        winrt::hstring m_sourcePath;
        winrt::hstring m_targetPath;

        int32_t m_currentIndex{ 2 };
        bool m_loadingSelection{ false };
        bool m_initialized{ false };
        bool m_workflowHooksInstalled{ false };
        bool m_windowClosingHookInstalled{ false };
        bool m_externalChangeAcknowledged{ false };
        bool m_hasUnsavedChanges{ false };
        bool m_workflowStateDirty{ false };
        bool m_lastSaveCreatedBackup{ false };
        bool m_lastSaveDetectedExternalChange{ false };
        bool m_forceSaveAsForRecoveredDraft{ false };
        bool m_hasTargetFileFingerprint{ false };
        uintmax_t m_targetFileSize{};
        int64_t m_targetFileTimestamp{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_workspaceDraftTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_externalChangeTimer{ nullptr };
        bool m_hasPendingHistoryText{ false };
        winrt::hstring m_pendingHistoryText;
        agi::winui::SubtitleFilter m_activeFilter{ agi::winui::SubtitleFilter::all };

        static constexpr size_t kMaxCpl = 42;
        static constexpr double kMaxCps = 20.0;

        void LoadCurrentRow();
        void UpdateMetrics();
        void UpdateSelectionVisuals();
        void StoreCurrentEditorSelection();
        void ScrollCurrentRowIntoView();
        void UpdateTableRow(int32_t index);
        int32_t RowIndexFromSender(winrt::Windows::Foundation::IInspectable const& sender) const;

        void InitializeDynamicSubtitleGrid();
        void RebuildSubtitleGrid();
        void HookWindowClosing();
        void StartExternalChangeMonitoring();
        void CheckForExternalTargetChange();
        void OpenProjectFiles();
        void OpenSourceFile();
        void OpenTargetFile();
        void OpenRecentProject();
        bool LoadRecentProjectPaths(std::wstring& source, std::wstring& target) const;
        void SaveRecentProjectPaths() const;
        void RefreshRecentProjectAction();
        void RefreshLoadedProject();
        void LoadWorkspaceState();
        bool SaveWorkspaceState();
        bool LoadWorkspaceDraft();
        bool SaveWorkspaceDraft();
        void DeleteWorkspaceDraft();
        void ScheduleWorkspaceDraftSave();
        void RefreshProjectFileLabels();
        void RefreshBackupAction();
        void SetDirty(bool dirty);
        bool ConfirmSaveBefore(std::wstring const& action);
        bool OfferSaveAsForExternalChange(std::wstring const& errorMessage);
        bool SelectSubtitleFile(std::wstring const& title, std::wstring& filename) const;
        bool SelectSubtitleSaveFile(std::wstring& filename) const;
        bool ReadSubtitleFile(
            std::wstring const& filename,
            std::vector<SubtitleEntry>& entries,
            std::wstring& errorMessage) const;
        void BuildAlignedRows();
        bool SaveTargetSubtitleFile(std::wstring& errorMessage, std::wstring const& destinationPath = {});

        void TargetTextBox_WorkflowKeyDown(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void RootGrid_WorkflowKeyDown(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void InitializeWorkflowStatuses();
        void RefreshQaAll();
        void RefreshProgressSummary();
        void RefreshCurrentQaVisuals();
        void RefreshApprovalAction();
        winrt::hstring EvaluateQaIssue(int32_t index) const;
        void CommitCurrentAndMoveNext(bool approve);
        void MoveCurrentBy(int32_t delta);
        void SaveFromShortcut();
        void SaveAsFromShortcut();
        void InsertLineBreakAtSelection();
        void ApplyEditHistory(bool redo);
        void UpdateDirtyFromRows();
        void MoveToProblem(int32_t direction);
        void MoveToUntranslated(int32_t direction);
        void MoveToReview(int32_t direction);
        void MoveToSearchResult(int32_t direction);
        void RefreshSearchSummary();
        bool RowMatchesSearch(SubtitleRowData const& row, std::wstring_view query) const;
        bool RowMatchesActiveFilter(SubtitleRowData const& row) const;
        void RefreshActiveFilter();
        void MoveToFilteredRow(int32_t direction);
        bool IsTranslationEmpty(winrt::hstring const& text) const;
        double WorkflowTimestampSeconds(winrt::hstring const& value) const;
    };

    inline double MainWindow::WorkflowTimestampSeconds(winrt::hstring const& value) const
    {
        std::wstring const text{ value.c_str() };
        if (text.size() < 12)
            return 0.0;

        try
        {
            auto const hours = std::stoi(text.substr(0, 2));
            auto const minutes = std::stoi(text.substr(3, 2));
            auto const seconds = std::stod(text.substr(6));
            return hours * 3600.0 + minutes * 60.0 + seconds;
        }
        catch (...)
        {
            return 0.0;
        }
    }

    inline void MainWindow::InitializeWorkflowStatuses()
    {
        for (auto& row : m_rows)
        {
            if (row.workflowStatus.empty())
                row.workflowStatus = row.status.empty() ? winrt::hstring{ L"P\u0159ipraveno" } : row.status;
            if (row.savedWorkflowStatus.empty())
                row.savedWorkflowStatus = row.workflowStatus;
            if (!row.historyInitialized)
            {
                row.savedTarget = row.target;
                row.historyInitialized = true;
            }
        }
    }

    inline winrt::hstring MainWindow::EvaluateQaIssue(int32_t index) const
    {
        if (m_targetPath.empty() || index < 0 || index >= static_cast<int32_t>(m_rows.size()))
            return L"";

        auto const& row = m_rows[index];
        std::wstring const text{ row.target.c_str() };
        std::wstring issues;
        auto addIssue = [&](std::wstring const& issue)
        {
            if (!issues.empty())
                issues += L"; ";
            issues += issue;
        };

        bool const emptyText = text.empty() || std::all_of(text.begin(), text.end(), [](wchar_t c)
        {
            return std::iswspace(c) != 0;
        });
        if (emptyText)
            addIssue(L"pr\u00E1zdn\u00FD \u010Desk\u00FD titulek");

        if (row.sourceStart.empty())
            addIssue(L"bez \u010Dasov\u00E9ho p\u00E1ru s origin\u00E1lem");

        if (!row.sourceStart.empty() && row.sourceMatchQuality > 0.0 && row.sourceMatchQuality < 0.35)
            addIssue(L"nejist\u00E9 \u010Dasov\u00E9 p\u00E1rov\u00E1n\u00ED");

        double const start = WorkflowTimestampSeconds(row.start);
        double const end = WorkflowTimestampSeconds(row.end);
        double const nextStart = index + 1 < static_cast<int32_t>(m_rows.size())
            ? WorkflowTimestampSeconds(m_rows[index + 1].start)
            : -1.0;
        auto const facts = agi::winui::AnalyzeSubtitleQuality(
            text, start, end, nextStart, kMaxCpl, kMaxCps);

        if (facts.invalid_interval)
            addIssue(L"neplatn\u00FD \u010Dasov\u00FD interval");
        else if (facts.too_short)
            addIssue(L"zobrazen\u00ED krat\u0161\u00ED ne\u017E 0,7 s");
        if (facts.too_many_lines)
            addIssue(L"v\u00EDce ne\u017E 2 \u0159\u00E1dky");
        if (facts.line_too_long)
            addIssue(L"CPL " + std::to_wstring(facts.max_line_length) + L" > " + std::to_wstring(kMaxCpl));

        if (facts.too_fast)
        {
            std::wostringstream stream;
            stream << L"CPS " << std::fixed << std::setprecision(1) << facts.cps << L" > " << kMaxCps;
            addIssue(stream.str());
        }
        if (facts.edge_whitespace)
            addIssue(L"mezera na za\u010D\u00E1tku nebo konci");
        if (facts.repeated_spaces)
            addIssue(L"opakovan\u00E9 mezery");
        if (facts.unbalanced_braces)
            addIssue(L"nevyv\u00E1\u017Een\u00E9 slo\u017Een\u00E9 z\u00E1vorky");
        if (facts.overlaps_next)
            addIssue(L"p\u0159ekryv s n\u00E1sleduj\u00EDc\u00EDm titulkem");

        return winrt::hstring{ issues };
    }

    inline void MainWindow::RefreshQaAll()
    {
        InitializeWorkflowStatuses();
        size_t issueCount = 0;
        for (int32_t i = 0; i < static_cast<int32_t>(m_rows.size()); ++i)
        {
            auto& row = m_rows[i];
            row.qaIssue = EvaluateQaIssue(i);
            if (!row.qaIssue.empty())
                ++issueCount;
            row.status = row.qaIssue.empty() ? row.workflowStatus : winrt::hstring{ L"Probl\u00E9m" };
            UpdateTableRow(i);
        }

        NextProblemButton().Content(winrt::box_value(winrt::hstring{
            L"Dal\u0161\u00ED probl\u00E9m (" + std::to_wstring(issueCount) + L")" }));
        NextProblemButton().IsEnabled(issueCount > 0);
        RefreshProgressSummary();
    }

    inline void MainWindow::RefreshCurrentQaVisuals()
    {
        if (m_rows.empty() || m_currentIndex < 0 || m_currentIndex >= static_cast<int32_t>(m_rows.size()))
            return;

        auto const& row = m_rows[m_currentIndex];
        std::wstring info = L"#" + std::to_wstring(row.number) + L" \u00B7 ";
        info += row.status.c_str();
        if (!row.qaIssue.empty())
        {
            info += L" \u00B7 ";
            info += row.qaIssue.c_str();
        }
        TargetInfoText().Text(winrt::hstring{ info });
        TargetStatusText().Text(winrt::hstring{ L"Stav: " + std::wstring(row.status.c_str()) });
        RefreshApprovalAction();

        if (!row.qaIssue.empty())
        {
            StatusBarText().Text(winrt::hstring{
                L"QA #" + std::to_wstring(row.number) + L" \u00B7 " + std::wstring(row.qaIssue.c_str()) });
        }
    }

    inline void MainWindow::MoveCurrentBy(int32_t delta)
    {
        MoveToFilteredRow(delta < 0 ? -1 : 1);
    }

    inline void MainWindow::CommitCurrentAndMoveNext(bool approve)
    {
        if (m_rows.empty())
            return;

        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size()))
            m_targetEntries[m_currentIndex].text = row.target;

        if (approve)
        {
            row.workflowStatus = L"Schv\u00E1leno";
            row.status = row.workflowStatus;
            if (!row.targetModified)
                row.savedWorkflowStatus = row.workflowStatus;
            m_workflowStateDirty = true;
            UpdateDirtyFromRows();
        }
        else if (row.targetModified)
        {
            row.workflowStatus = L"Upraveno";
            row.status = row.workflowStatus;
        }
        else if (row.workflowStatus.empty())
        {
            row.workflowStatus = row.status == L"Probl\u00E9m" ? winrt::hstring{ L"P\u0159ipraveno" } : row.status;
        }

        RefreshQaAll();
        UpdateTableRow(m_currentIndex);

        MoveToFilteredRow(1);
        RefreshCurrentQaVisuals();
        TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    inline void MainWindow::SaveFromShortcut()
    {
        if (m_rows.empty())
            return;

        if (m_targetPath.empty())
        {
            SaveAsFromShortcut();
            return;
        }

        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size()))
            m_targetEntries[m_currentIndex].text = row.target;

        std::wstring errorMessage;
        if (!SaveTargetSubtitleFile(errorMessage))
        {
            StatusBarText().Text(L"Ulo\u017Een\u00ED \u010Desk\u00FDch titulk\u016F se nezda\u0159ilo");
            if (m_lastSaveDetectedExternalChange)
            {
                OfferSaveAsForExternalChange(errorMessage);
                return;
            }
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
        auto const backupInfo = m_lastSaveCreatedBackup ? std::wstring{ L" \u00B7 z\u00E1loha v LocalAppData" } : std::wstring{};
        StatusBarText().Text(winrt::hstring{
            L"\u010Ce\u0161tina ulo\u017Eena \u00B7 " + targetName + backupInfo + L" \u00B7 Ctrl+S" });
    }

    inline void MainWindow::SaveAsFromShortcut()
    {
        if (m_rows.empty())
            return;

        std::wstring destination;
        if (!SelectSubtitleSaveFile(destination))
            return;

        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size()))
            m_targetEntries[m_currentIndex].text = row.target;

        std::wstring errorMessage;
        if (!SaveTargetSubtitleFile(errorMessage, destination))
        {
            StatusBarText().Text(L"Ulo\u017Een\u00ED \u010Desk\u00FDch titulk\u016F se nezda\u0159ilo");
            if (m_lastSaveDetectedExternalChange)
            {
                OfferSaveAsForExternalChange(errorMessage);
                return;
            }
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
        auto const backupInfo = m_lastSaveCreatedBackup ? std::wstring{ L" \u00B7 z\u00E1loha v LocalAppData" } : std::wstring{};
        StatusBarText().Text(winrt::hstring{
            L"\u010Ce\u0161tina ulo\u017Eena jako \u00B7 " + targetName + backupInfo });
    }

    inline void MainWindow::TargetTextBox_WorkflowKeyDown(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        bool const shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool const control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        auto const key = args.Key();

        if (key == winrt::Windows::System::VirtualKey::Enter)
        {
            args.Handled(true);
            if (shift)
                InsertLineBreakAtSelection();
            else
                CommitCurrentAndMoveNext(control);
            return;
        }

        if (key == winrt::Windows::System::VirtualKey::PageUp)
        {
            args.Handled(true);
            MoveCurrentBy(-1);
            return;
        }
        if (key == winrt::Windows::System::VirtualKey::PageDown)
        {
            args.Handled(true);
            MoveCurrentBy(1);
            return;
        }
        if (control && key == winrt::Windows::System::VirtualKey::S)
        {
            args.Handled(true);
            if (shift)
                SaveAsFromShortcut();
            else
                SaveFromShortcut();
            return;
        }
        if (control && key == winrt::Windows::System::VirtualKey::Z)
        {
            args.Handled(true);
            ApplyEditHistory(shift);
            return;
        }
        if (control && key == winrt::Windows::System::VirtualKey::Y)
        {
            args.Handled(true);
            ApplyEditHistory(true);
        }
    }

    inline void MainWindow::RootGrid_WorkflowKeyDown(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        bool const control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool const shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        auto const key = args.Key();

        if (key == winrt::Windows::System::VirtualKey::PageUp)
        {
            args.Handled(true);
            MoveCurrentBy(-1);
        }
        else if (key == winrt::Windows::System::VirtualKey::PageDown)
        {
            args.Handled(true);
            MoveCurrentBy(1);
        }
        else if (control && key == winrt::Windows::System::VirtualKey::S)
        {
            args.Handled(true);
            if (shift)
                SaveAsFromShortcut();
            else
                SaveFromShortcut();
        }
        else if (control && key == winrt::Windows::System::VirtualKey::O)
        {
            args.Handled(true);
            OpenProjectFiles();
        }
        else if (control && key == winrt::Windows::System::VirtualKey::F)
        {
            args.Handled(true);
            SearchTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            SearchTextBox().SelectAll();
        }
        else if (key == winrt::Windows::System::VirtualKey::F3)
        {
            args.Handled(true);
            MoveToSearchResult(shift ? -1 : 1);
        }
        else if (key == winrt::Windows::System::VirtualKey::F6)
        {
            args.Handled(true);
            MoveToReview(shift ? -1 : 1);
        }
        else if (key == winrt::Windows::System::VirtualKey::F7)
        {
            args.Handled(true);
            MoveToUntranslated(shift ? -1 : 1);
        }
        else if (key == winrt::Windows::System::VirtualKey::F8)
        {
            args.Handled(true);
            MoveToProblem(shift ? -1 : 1);
        }
    }

    inline void MainWindow::TargetTextBox_WorkflowLoaded(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_workflowHooksInstalled)
            return;
        m_workflowHooksInstalled = true;

        // TextBox class handling may consume Enter before a bubbling KeyDown
        // subscription runs. PreviewKeyDown lets the workflow decide whether
        // Enter is a commit or an explicit line break before TextBox edits text.
        TargetTextBox().PreviewKeyDown({ this, &MainWindow::TargetTextBox_WorkflowKeyDown });
        RootGrid().PreviewKeyDown({ this, &MainWindow::RootGrid_WorkflowKeyDown });

        TargetTextBox().TextChanged([this](auto const&, auto const&)
        {
            if (m_loadingSelection || !m_initialized || m_rows.empty())
                return;
            auto& row = m_rows[m_currentIndex];
            if (row.status != L"Probl\u00E9m")
                row.workflowStatus = row.status;
            RefreshQaAll();
            RefreshCurrentQaVisuals();
        });

        PreviousButton().Click([this](auto const&, auto const&)
        {
            RefreshCurrentQaVisuals();
        });
        NextButton().Click([this](auto const&, auto const&)
        {
            RefreshCurrentQaVisuals();
        });

        InitializeWorkflowStatuses();
        RefreshQaAll();
        RefreshCurrentQaVisuals();
    }

    inline void MainWindow::InsertLineBreakButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const box = TargetTextBox();
        std::wstring const original{ box.Text().c_str() };
        auto const balanced = agi::winui::RebalanceSubtitleText(original);
        if (balanced == original)
        {
            StatusBarText().Text(L"Zalomen\u00ED beze zm\u011Bny \u00B7 text je ji\u017E vyv\u00E1\u017Een\u00FD nebo nem\u00E1 vhodnou mezeru");
            box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }

        auto const lineBreak = balanced.find(L"\r\n");
        box.Text(winrt::hstring{ balanced });
        box.SelectionStart(static_cast<int32_t>(lineBreak + 2));
        box.SelectionLength(0);
        box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    inline void MainWindow::CopyOriginalButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_rows.empty())
            return;

        auto const& original = m_rows[m_currentIndex].original;
        if (original.empty())
        {
            StatusBarText().Text(L"Aktu\u00E1ln\u00ED titulek nem\u00E1 sp\u00E1rovan\u00FD origin\u00E1ln\u00ED text");
            return;
        }

        auto const box = TargetTextBox();
        if (box.Text() == original)
        {
            StatusBarText().Text(L"\u010Cesk\u00FD text ji\u017E odpov\u00EDd\u00E1 origin\u00E1lu");
            box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }

        box.Text(original);
        box.SelectionStart(static_cast<int32_t>(original.size()));
        box.SelectionLength(0);
        box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
        StatusBarText().Text(L"Origin\u00E1ln\u00ED text p\u0159evzat \u00B7 Ctrl+Z vr\u00E1t\u00ED p\u0159edchoz\u00ED p\u0159eklad");
    }

    inline void MainWindow::InsertLineBreakAtSelection()
    {
        auto const box = TargetTextBox();
        std::wstring text{ box.Text().c_str() };
        auto const selectionStart = static_cast<size_t>((std::max)(0, box.SelectionStart()));
        auto const selectionLength = static_cast<size_t>((std::max)(0, box.SelectionLength()));
        auto const safeStart = (std::min)(selectionStart, text.size());
        auto const safeLength = (std::min)(selectionLength, text.size() - safeStart);

        text.replace(safeStart, safeLength, L"\r\n");
        box.Text(winrt::hstring{ text });
        box.SelectionStart(static_cast<int32_t>(safeStart + 2));
        box.SelectionLength(0);
        box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    inline void MainWindow::ApplyEditHistory(bool redo)
    {
        if (m_rows.empty())
            return;

        auto& row = m_rows[m_currentIndex];
        auto& source = redo ? row.redoHistory : row.undoHistory;
        auto& destination = redo ? row.undoHistory : row.redoHistory;
        if (source.empty())
        {
            StatusBarText().Text(redo
                ? L"Nen\u00ED k dispozici \u017E\u00E1dn\u00E1 zm\u011Bna k opakov\u00E1n\u00ED"
                : L"Nen\u00ED k dispozici \u017E\u00E1dn\u00E1 zm\u011Bna k vr\u00E1cen\u00ED");
            return;
        }

        destination.push_back(row.target);
        auto const nextText = source.back();
        source.pop_back();
        row.editSequenceKind = 0;

        m_pendingHistoryText = nextText;
        m_hasPendingHistoryText = true;
        auto const box = TargetTextBox();
        box.Text(nextText);
        box.SelectionStart(static_cast<int32_t>(nextText.size()));
        box.SelectionLength(0);
        box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
        StatusBarText().Text(redo ? L"Zm\u011Bna zopakov\u00E1na \u00B7 Ctrl+Y" : L"Zm\u011Bna vr\u00E1cena \u00B7 Ctrl+Z");
    }

    inline void MainWindow::UpdateDirtyFromRows()
    {
        SetDirty(m_workflowStateDirty || std::any_of(m_rows.begin(), m_rows.end(), [](auto const& row)
        {
            return row.targetModified;
        }));
        ScheduleWorkspaceDraftSave();
    }

    inline void MainWindow::NextProblemButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        MoveToProblem(1);
    }

    inline void MainWindow::NextUntranslatedButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        MoveToUntranslated(1);
    }

    inline void MainWindow::NextReviewButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        MoveToReview(1);
    }

    inline void MainWindow::MoveToReview(int32_t direction)
    {
        if (m_rows.empty() || direction == 0)
            return;

        auto const rowCount = static_cast<int32_t>(m_rows.size());
        for (int32_t offset = 1; offset <= rowCount; ++offset)
        {
            auto index = (m_currentIndex + direction * offset) % rowCount;
            if (index < 0)
                index += rowCount;
            auto const& row = m_rows[index];
            if (row.workflowStatus == L"Schv\u00E1leno" && row.qaIssue.empty())
                continue;

            if (index != m_currentIndex)
            {
                StoreCurrentEditorSelection();
                m_currentIndex = index;
                LoadCurrentRow();
                RefreshCurrentQaVisuals();
            }
            StatusBarText().Text(winrt::hstring{
                L"Titulek ke kontrole #" + std::to_wstring(row.number) + L" \u00B7 F6" });
            TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }

        StatusBarText().Text(L"V\u0161echny titulky jsou schv\u00E1len\u00E9 a bez probl\u00E9m\u016F");
    }

    inline void MainWindow::MoveToUntranslated(int32_t direction)
    {
        if (m_rows.empty() || direction == 0)
            return;

        auto const rowCount = static_cast<int32_t>(m_rows.size());
        for (int32_t offset = 1; offset <= rowCount; ++offset)
        {
            auto index = (m_currentIndex + direction * offset) % rowCount;
            if (index < 0)
                index += rowCount;
            if (!IsTranslationEmpty(m_rows[index].target))
                continue;

            if (index != m_currentIndex)
            {
                StoreCurrentEditorSelection();
                m_currentIndex = index;
                LoadCurrentRow();
                RefreshCurrentQaVisuals();
            }
            StatusBarText().Text(winrt::hstring{
                L"Nep\u0159elo\u017Een\u00FD titulek #" + std::to_wstring(m_rows[index].number) + L" \u00B7 F7" });
            TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }

        StatusBarText().Text(L"V\u0161echny titulky obsahuj\u00ED p\u0159eklad");
    }

    inline void MainWindow::MoveToProblem(int32_t direction)
    {
        if (m_rows.empty() || direction == 0)
            return;

        RefreshQaAll();
        auto const rowCount = static_cast<int32_t>(m_rows.size());
        for (int32_t offset = 1; offset <= rowCount; ++offset)
        {
            auto index = (m_currentIndex + direction * offset) % rowCount;
            if (index < 0)
                index += rowCount;
            if (m_rows[index].qaIssue.empty())
                continue;

            StoreCurrentEditorSelection();
            m_currentIndex = index;
            LoadCurrentRow();
            RefreshCurrentQaVisuals();
            TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }

        StatusBarText().Text(L"QA \u00B7 nebyl nalezen \u017E\u00E1dn\u00FD probl\u00E9m");
    }
}

namespace winrt::Aegisub_WinUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
