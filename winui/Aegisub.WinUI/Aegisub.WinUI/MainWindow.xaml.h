#pragma once

#include "MainWindow.g.h"
#include "../../../src/winui_bridge_text.h"
#include "../../../src/winui_project_state.h"
#include "../../../src/winui_search_replace.h"
#include "../../../src/winui_subtitle_io.h"
#include "../../../src/winui_subtitle_qa.h"
#include "../../../src/winui_subtitle_workflow.h"

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>
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

        void RootGrid_Loaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void PreviousButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void NextButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ApproveButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SaveButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SaveAsButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RestoreBackupButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OpenBothButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OpenSourceButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OpenTargetButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OpenRecentProjectButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RecoveryOverviewButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OpenRecoveryFolderButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void DeleteRecoveryFilesButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void TargetTextBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);
        void SubtitleRow_Tapped(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& args);
        void TargetTextBox_WorkflowLoaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void InsertLineBreakButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void CopyOriginalButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void NextProblemButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void NextUntranslatedButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void NextReviewButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SearchTextBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);
        void SearchTextBox_KeyDown(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void SearchPreviousButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SearchNextButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void FilterComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void ReplacePreviewButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ReplaceAllButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void UndoBulkButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ConsistencyCheckButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void BulkReadyButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void BulkApprovedButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        // Translation-workspace features kept outside MainWindow.xaml.cpp.
        void MergeLinesButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SearchOptions_Changed(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void BulkNormalizeButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ReloadProjectButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ApplySettingsButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void PairPreviousButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void PairNextButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void PairIgnoreButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RecentProjectsButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void AdvancedSearchTextBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);
        void AdvancedSearchTextBox_KeyDown(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void AdvancedSearchPreviousButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void AdvancedSearchNextButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void AdvancedReplacePreviewButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void AdvancedReplaceAllButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SearchScopeComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OpenSelectedRecentProjectButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RefreshRecoveryHistoryButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RestoreSelectedRecoveryButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RootGrid_DragOver(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::DragEventArgs const& args);
        void RootGrid_Drop(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::DragEventArgs const& args);

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
            int32_t manualSourceIndex{-1};
            bool pairingIgnored{};
        };

        struct BulkRowSnapshot
        {
            size_t index{};
            winrt::hstring target;
            winrt::hstring workflowStatus;
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
        std::uintmax_t m_targetFileSize{};
        int64_t m_targetFileTimestamp{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_workspaceDraftTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_externalChangeTimer{ nullptr };
        bool m_hasPendingHistoryText{ false };
        winrt::hstring m_pendingHistoryText;
        agi::winui::SubtitleFilter m_activeFilter{ agi::winui::SubtitleFilter::all };
        std::vector<BulkRowSnapshot> m_lastBulkSnapshot;
        winrt::hstring m_lastBulkAction;
        bool m_lastBulkWorkflowStateDirty{};
        agi::winui::WinUiWorkspaceSettings m_workspaceSettings;
        std::vector<agi::winui::RecentTranslationProject> m_recentProjects;
        std::vector<std::wstring> m_recoveryVersions;
        bool m_featureStateLoaded{};

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
        bool ReadSubtitleFile(std::wstring const& filename, std::vector<SubtitleEntry>& entries,
            std::wstring& errorMessage) const;
        void BuildAlignedRows();
        bool SaveTargetSubtitleFile(std::wstring& errorMessage, std::wstring const& destinationPath = {});

        void TargetTextBox_WorkflowKeyDown(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void RootGrid_WorkflowKeyDown(winrt::Windows::Foundation::IInspectable const& sender,
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
        void SelectFilter(int32_t index);
        void RefreshProjectOverview();
        std::vector<size_t> VisibleRowIndices() const;
        size_t ReplacementCount(std::wstring_view query) const;
        void ApplyBulkStatus(winrt::hstring const& status);
        void CaptureBulkSnapshot(std::vector<size_t> const& indices, winrt::hstring const& action);
        void RestoreLastBulkSnapshot();
        void ClearBulkUndo();
        bool IsTranslationEmpty(winrt::hstring const& text) const;
        double WorkflowTimestampSeconds(winrt::hstring const& value) const;

        agi::winui::SearchOptions CurrentSearchOptions() const;
        void RefreshFeatureMetrics();
        void LoadFeatureState();
        void SaveFeatureSettings() const;
        void RememberRecentProject();
        void RefreshPairingUi();
        void ChangeManualPair(int32_t delta);
        void CaptureRecoveryHistorySnapshot() const;

        bool RowMatchesAdvancedSearch(SubtitleRowData const& row, std::wstring_view query) const;
        void RefreshAdvancedSearchSummary();
        void MoveToAdvancedSearchResult(int32_t direction);
        void ApplyAdvancedReplace(bool previewOnly);
        void RefreshFeatureOverview();
        void RefreshRecentProjectsUi();
        void OpenRecentProjectAt(size_t index);
        void RefreshRecoveryHistoryUi();
        void RestoreRecoveryVersion(size_t index);
        winrt::fire_and_forget OpenDroppedProjectAsync(winrt::Microsoft::UI::Xaml::DragEventArgs args);
    };
}

#include "MainWindow.Workflow.inl"
#include "MainWindow.Advanced.inl"

namespace winrt::Aegisub_WinUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
