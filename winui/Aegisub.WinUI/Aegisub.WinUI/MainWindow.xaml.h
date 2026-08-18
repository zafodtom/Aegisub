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

        void NextProblemButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

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
            winrt::hstring savedTarget;
            std::vector<winrt::hstring> undoHistory;
            std::vector<winrt::hstring> redoHistory;
            bool historyInitialized{};
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
        winrt::hstring m_sourcePath;
        winrt::hstring m_targetPath;

        int32_t m_currentIndex{ 2 };
        bool m_loadingSelection{ false };
        bool m_initialized{ false };
        bool m_workflowHooksInstalled{ false };
        bool m_windowClosingHookInstalled{ false };
        bool m_hasUnsavedChanges{ false };
        bool m_applyingEditHistory{ false };

        static constexpr size_t kMaxCpl = 42;
        static constexpr double kMaxCps = 20.0;

        void LoadCurrentRow();
        void UpdateMetrics();
        void UpdateSelectionVisuals();
        void UpdateTableRow(int32_t index);
        int32_t RowIndexFromSender(winrt::Windows::Foundation::IInspectable const& sender) const;

        void InitializeDynamicSubtitleGrid();
        void RebuildSubtitleGrid();
        void HookOpenProjectButton();
        void HookWindowClosing();
        void OpenProjectFiles();
        void SetDirty(bool dirty);
        bool ConfirmSaveBefore(std::wstring const& action);
        bool SelectSubtitleFile(std::wstring const& title, std::wstring& filename) const;
        bool ReadSubtitleFile(
            std::wstring const& filename,
            std::vector<SubtitleEntry>& entries,
            std::wstring& errorMessage) const;
        void BuildAlignedRows();
        bool SaveTargetSubtitleFile(std::wstring& errorMessage);

        winrt::Microsoft::UI::Xaml::Controls::Button FindOpenProjectButton(
            winrt::Microsoft::UI::Xaml::DependencyObject const& root) const;

        void TargetTextBox_WorkflowKeyDown(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void RootGrid_WorkflowKeyDown(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void InitializeWorkflowStatuses();
        void SyncWorkflowStatusesFromDisplay();
        void RefreshQaAll();
        void RefreshCurrentQaVisuals();
        winrt::hstring EvaluateQaIssue(int32_t index) const;
        void CommitCurrentAndMoveNext(bool approve);
        void MoveCurrentBy(int32_t delta);
        void SaveFromShortcut();
        void InsertLineBreakAtSelection();
        void ApplyEditHistory(bool redo);
        void UpdateDirtyFromRows();
        double WorkflowTimestampSeconds(winrt::hstring const& value) const;
        winrt::Microsoft::UI::Xaml::Controls::Button FindWorkflowButton(
            winrt::Microsoft::UI::Xaml::DependencyObject const& root,
            winrt::hstring const& content) const;
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

    inline winrt::Microsoft::UI::Xaml::Controls::Button MainWindow::FindWorkflowButton(
        winrt::Microsoft::UI::Xaml::DependencyObject const& root,
        winrt::hstring const& content) const
    {
        using namespace winrt::Microsoft::UI::Xaml::Controls;
        if (!root)
            return nullptr;

        if (auto const button = root.try_as<Button>())
        {
            try
            {
                if (winrt::unbox_value<winrt::hstring>(button.Content()) == content)
                    return button;
            }
            catch (...)
            {
            }
        }

        auto const count = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < count; ++i)
        {
            auto const child = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChild(root, i);
            if (auto const found = FindWorkflowButton(child, content))
                return found;
        }
        return nullptr;
    }

    inline void MainWindow::InitializeWorkflowStatuses()
    {
        for (auto& row : m_rows)
        {
            if (row.workflowStatus.empty())
                row.workflowStatus = row.status.empty() ? winrt::hstring{ L"P\u0159ipraveno" } : row.status;
            if (!row.historyInitialized)
            {
                row.savedTarget = row.target;
                row.historyInitialized = true;
            }
        }
    }

    inline void MainWindow::SyncWorkflowStatusesFromDisplay()
    {
        for (auto& row : m_rows)
        {
            if (row.status != L"Probl\u00E9m" && !row.status.empty())
                row.workflowStatus = row.status;
            else if (row.workflowStatus.empty())
                row.workflowStatus = L"P\u0159ipraveno";
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

        if (row.duration <= 0.0)
            addIssue(L"neplatn\u00E1 d\u00E9lka");

        size_t lineCount = 1;
        size_t currentLineLength = 0;
        size_t maxLineLength = 0;
        size_t characterCount = 0;
        for (wchar_t c : text)
        {
            if (c == L'\r')
                continue;
            if (c == L'\n')
            {
                ++lineCount;
                maxLineLength = (std::max)(maxLineLength, currentLineLength);
                currentLineLength = 0;
                continue;
            }
            ++currentLineLength;
            ++characterCount;
        }
        maxLineLength = (std::max)(maxLineLength, currentLineLength);

        if (lineCount > 2)
            addIssue(L"v\u00EDce ne\u017E 2 \u0159\u00E1dky");
        if (maxLineLength > kMaxCpl)
            addIssue(L"CPL " + std::to_wstring(maxLineLength) + L" > " + std::to_wstring(kMaxCpl));

        double const cps = row.duration > 0.0 ? static_cast<double>(characterCount) / row.duration : 0.0;
        if (cps > kMaxCps)
        {
            std::wostringstream stream;
            stream << L"CPS " << std::fixed << std::setprecision(1) << cps << L" > " << kMaxCps;
            addIssue(stream.str());
        }

        if (index + 1 < static_cast<int32_t>(m_rows.size()))
        {
            double const currentEnd = WorkflowTimestampSeconds(row.end);
            double const nextStart = WorkflowTimestampSeconds(m_rows[index + 1].start);
            if (currentEnd > nextStart + 0.0005)
                addIssue(L"p\u0159ekryv s n\u00E1sleduj\u00EDc\u00EDm titulkem");
        }

        return winrt::hstring{ issues };
    }

    inline void MainWindow::RefreshQaAll()
    {
        InitializeWorkflowStatuses();
        for (int32_t i = 0; i < static_cast<int32_t>(m_rows.size()); ++i)
        {
            auto& row = m_rows[i];
            row.qaIssue = EvaluateQaIssue(i);
            row.status = row.qaIssue.empty() ? row.workflowStatus : winrt::hstring{ L"Probl\u00E9m" };
            UpdateTableRow(i);
        }
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

        if (!row.qaIssue.empty())
        {
            StatusBarText().Text(winrt::hstring{
                L"QA #" + std::to_wstring(row.number) + L" \u00B7 " + std::wstring(row.qaIssue.c_str()) });
        }
    }

    inline void MainWindow::MoveCurrentBy(int32_t delta)
    {
        if (m_rows.empty())
            return;

        auto const candidate = m_currentIndex + delta;
        auto const last = static_cast<int32_t>(m_rows.size()) - 1;
        auto const next = (std::max)(0, (std::min)(last, candidate));
        if (next == m_currentIndex)
            return;

        m_currentIndex = next;
        LoadCurrentRow();
        RefreshCurrentQaVisuals();
        TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
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

        if (m_currentIndex < static_cast<int32_t>(m_rows.size()) - 1)
        {
            ++m_currentIndex;
            LoadCurrentRow();
        }
        RefreshCurrentQaVisuals();
        TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    inline void MainWindow::SaveFromShortcut()
    {
        if (m_rows.empty())
            return;

        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size()))
            m_targetEntries[m_currentIndex].text = row.target;

        std::wstring errorMessage;
        if (!SaveTargetSubtitleFile(errorMessage))
        {
            StatusBarText().Text(L"Ulo\u017Een\u00ED \u010Desk\u00FDch titulk\u016F se nezda\u0159ilo");
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
        StatusBarText().Text(winrt::hstring{ L"\u010Ce\u0161tina ulo\u017Eena \u00B7 " + targetName + L" \u00B7 Ctrl+S" });
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
            SaveFromShortcut();
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

        if (auto const approve = FindWorkflowButton(RootGrid(), L"Schv\u00E1lit"))
        {
            approve.Click([this](auto const&, auto const&)
            {
                SyncWorkflowStatusesFromDisplay();
                RefreshQaAll();
                RefreshCurrentQaVisuals();
            });
        }

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

        m_applyingEditHistory = true;
        auto const box = TargetTextBox();
        box.Text(nextText);
        m_applyingEditHistory = false;
        box.SelectionStart(static_cast<int32_t>(nextText.size()));
        box.SelectionLength(0);
        box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
        StatusBarText().Text(redo ? L"Zm\u011Bna zopakov\u00E1na \u00B7 Ctrl+Y" : L"Zm\u011Bna vr\u00E1cena \u00B7 Ctrl+Z");
    }

    inline void MainWindow::UpdateDirtyFromRows()
    {
        SetDirty(std::any_of(m_rows.begin(), m_rows.end(), [](auto const& row)
        {
            return row.targetModified;
        }));
    }

    inline void MainWindow::NextProblemButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_rows.empty())
            return;

        RefreshQaAll();
        for (size_t offset = 1; offset <= m_rows.size(); ++offset)
        {
            auto const index = static_cast<int32_t>((static_cast<size_t>(m_currentIndex) + offset) % m_rows.size());
            if (m_rows[index].qaIssue.empty())
                continue;

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
