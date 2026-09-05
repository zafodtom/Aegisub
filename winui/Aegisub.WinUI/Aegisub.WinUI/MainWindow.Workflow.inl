#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>

namespace winrt::Aegisub_WinUI::implementation
{
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
        catch (...) { return 0.0; }
    }

    inline void MainWindow::InitializeWorkflowStatuses()
    {
        for (auto& row : m_rows)
        {
            if (row.workflowStatus.empty())
                row.workflowStatus = row.status.empty() ? winrt::hstring{ L"Připraveno" } : row.status;
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
        std::wstring issues;
        auto addIssue = [&](std::wstring const& issue)
        {
            if (!issues.empty()) issues += L"; ";
            issues += issue;
        };

        if (!row.pairingIgnored)
        {
            if (row.sourceStart.empty())
                addIssue(L"bez časového páru s originálem");
            else if (row.manualSourceIndex < 0 && row.sourceMatchQuality > 0.0 && row.sourceMatchQuality < 0.35)
                addIssue(L"nejisté časové párování");
        }

        auto const start = WorkflowTimestampSeconds(row.start);
        auto const end = WorkflowTimestampSeconds(row.end);
        auto const nextStart = index + 1 < static_cast<int32_t>(m_rows.size())
            ? WorkflowTimestampSeconds(m_rows[index + 1].start) : -1.0;
        auto const report = agi::winui::AnalyzeTranslationQuality(
            std::wstring_view{ row.original.c_str(), row.original.size() },
            std::wstring_view{ row.target.c_str(), row.target.size() },
            start, end, nextStart, m_workspaceSettings.qa);
        for (auto const issue : report.issues)
            addIssue(agi::winui::SubtitleQaIssueLabel(issue));
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
            if (!row.qaIssue.empty()) ++issueCount;
            row.status = row.qaIssue.empty() ? row.workflowStatus : winrt::hstring{ L"Problém" };
            UpdateTableRow(i);
        }
        NextProblemButton().Content(winrt::box_value(winrt::hstring{
            L"Další problém (" + std::to_wstring(issueCount) + L")" }));
        NextProblemButton().IsEnabled(issueCount > 0);
        RefreshProgressSummary();
        RefreshProjectOverview();
    }

    inline void MainWindow::RefreshFeatureMetrics()
    {
        if (m_rows.empty() || m_currentIndex < 0 || m_currentIndex >= static_cast<int32_t>(m_rows.size()))
            return;
        auto const& row = m_rows[m_currentIndex];
        auto const start = WorkflowTimestampSeconds(row.start);
        auto const end = WorkflowTimestampSeconds(row.end);
        auto const nextStart = m_currentIndex + 1 < static_cast<int32_t>(m_rows.size())
            ? WorkflowTimestampSeconds(m_rows[m_currentIndex + 1].start) : -1.0;
        auto const report = agi::winui::AnalyzeTranslationQuality(
            std::wstring_view{ row.original.c_str(), row.original.size() },
            std::wstring_view{ row.target.c_str(), row.target.size() }, start, end, nextStart, m_workspaceSettings.qa);

        TargetCplText().Text(winrt::hstring{ L"CPL " + std::to_wstring(report.facts.max_line_length) +
            L"/" + std::to_wstring(m_workspaceSettings.qa.maximum_cpl) });
        std::wostringstream cps;
        cps << L"CPS " << std::fixed << std::setprecision(1) << report.facts.cps << L"/"
            << m_workspaceSettings.qa.maximum_cps;
        TargetCpsText().Text(winrt::hstring{ cps.str() });
        TargetLengthText().Text(winrt::hstring{ L"Znaky " + std::to_wstring(report.facts.character_count) +
            L" · " + std::to_wstring(report.facts.line_count) + L" ř." });
        RefreshPairingUi();
    }

    inline void MainWindow::RefreshCurrentQaVisuals()
    {
        if (m_rows.empty() || m_currentIndex < 0 || m_currentIndex >= static_cast<int32_t>(m_rows.size()))
            return;
        auto const& row = m_rows[m_currentIndex];
        std::wstring info = L"#" + std::to_wstring(row.number) + L" · " + std::wstring{ row.status.c_str() };
        if (!row.qaIssue.empty()) info += L" · " + std::wstring{ row.qaIssue.c_str() };
        TargetInfoText().Text(winrt::hstring{ info });
        TargetStatusText().Text(winrt::hstring{ L"Stav: " + std::wstring(row.status.c_str()) });
        RefreshApprovalAction();
        RefreshFeatureMetrics();
        if (!row.qaIssue.empty())
            StatusBarText().Text(winrt::hstring{ L"QA #" + std::to_wstring(row.number) + L" · " + std::wstring(row.qaIssue.c_str()) });
    }

    inline void MainWindow::MoveCurrentBy(int32_t delta) { MoveToFilteredRow(delta < 0 ? -1 : 1); }

    inline void MainWindow::CommitCurrentAndMoveNext(bool approve)
    {
        if (m_rows.empty()) return;
        ClearBulkUndo();
        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size())) m_targetEntries[m_currentIndex].text = row.target;
        if (approve)
        {
            row.workflowStatus = L"Schváleno";
            row.status = row.workflowStatus;
            if (!row.targetModified) row.savedWorkflowStatus = row.workflowStatus;
            m_workflowStateDirty = true;
            UpdateDirtyFromRows();
        }
        else if (row.targetModified)
        {
            row.workflowStatus = L"Upraveno";
            row.status = row.workflowStatus;
        }
        else if (row.workflowStatus.empty())
            row.workflowStatus = row.status == L"Problém" ? winrt::hstring{ L"Připraveno" } : row.status;
        RefreshQaAll();
        UpdateTableRow(m_currentIndex);
        MoveToFilteredRow(1);
        RefreshCurrentQaVisuals();
        TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    inline void MainWindow::CaptureRecoveryHistorySnapshot() const
    {
        if (m_targetPath.empty()) return;
        try
        {
            auto const* local = _wgetenv(L"LOCALAPPDATA");
            if (!local) return;
            std::filesystem::path const source{ m_targetPath.c_str() };
            if (!std::filesystem::exists(source)) return;
            auto const directory = std::filesystem::path{ local } / L"Aegisub" / L"TranslationWorkspace" / L"Backups";
            std::filesystem::create_directories(directory);
            auto const stamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            auto destination = directory / (source.filename().wstring() + L"." + std::to_wstring(stamp) + L".bak");
            std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);

            std::vector<std::filesystem::directory_entry> entries;
            for (auto const& entry : std::filesystem::directory_iterator(directory))
                if (entry.is_regular_file()) entries.push_back(entry);
            std::sort(entries.begin(), entries.end(), [](auto const& a, auto const& b) {
                return a.last_write_time() > b.last_write_time();
            });
            for (size_t i = 20; i < entries.size(); ++i)
            {
                std::error_code error;
                std::filesystem::remove(entries[i].path(), error);
            }
        }
        catch (...) {}
    }

    inline void MainWindow::SaveFromShortcut()
    {
        if (m_rows.empty()) return;
        if (m_targetPath.empty()) { SaveAsFromShortcut(); return; }
        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size())) m_targetEntries[m_currentIndex].text = row.target;
        std::wstring errorMessage;
        if (!SaveTargetSubtitleFile(errorMessage))
        {
            StatusBarText().Text(L"Uložení českých titulků se nezdařilo");
            if (m_lastSaveDetectedExternalChange) { OfferSaveAsForExternalChange(errorMessage); return; }
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }
        CaptureRecoveryHistorySnapshot();
        RememberRecentProject();
        auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
        auto const backupInfo = m_lastSaveCreatedBackup ? std::wstring{ L" · záloha v LocalAppData" } : std::wstring{};
        StatusBarText().Text(winrt::hstring{ L"Čeština uložena · " + targetName + backupInfo + L" · projekt je čistý" });
    }

    inline void MainWindow::SaveAsFromShortcut()
    {
        if (m_rows.empty()) return;
        std::wstring destination;
        if (!SelectSubtitleSaveFile(destination)) return;
        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size())) m_targetEntries[m_currentIndex].text = row.target;
        std::wstring errorMessage;
        if (!SaveTargetSubtitleFile(errorMessage, destination))
        {
            StatusBarText().Text(L"Uložení českých titulků se nezdařilo");
            if (m_lastSaveDetectedExternalChange) { OfferSaveAsForExternalChange(errorMessage); return; }
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }
        CaptureRecoveryHistorySnapshot();
        RememberRecentProject();
        auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
        StatusBarText().Text(winrt::hstring{ L"Čeština uložena jako · " + targetName + L" · projekt je čistý" });
    }

    inline void MainWindow::TargetTextBox_WorkflowKeyDown(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        bool const shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool const control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        auto const key = args.Key();
        auto const keyValue = static_cast<int32_t>(key);
        if (control && keyValue >= 0x31 && keyValue <= 0x36) { args.Handled(true); SelectFilter(keyValue - 0x31); return; }
        if (key == winrt::Windows::System::VirtualKey::Enter)
        {
            args.Handled(true);
            if (shift) InsertLineBreakAtSelection(); else CommitCurrentAndMoveNext(control);
            return;
        }
        if (key == winrt::Windows::System::VirtualKey::PageUp) { args.Handled(true); MoveCurrentBy(-1); return; }
        if (key == winrt::Windows::System::VirtualKey::PageDown) { args.Handled(true); MoveCurrentBy(1); return; }
        if (control && key == winrt::Windows::System::VirtualKey::S)
        { args.Handled(true); if (shift) SaveAsFromShortcut(); else SaveFromShortcut(); return; }
        if (control && key == winrt::Windows::System::VirtualKey::Z)
        { args.Handled(true); ApplyEditHistory(shift); return; }
        if (control && key == winrt::Windows::System::VirtualKey::Y)
        { args.Handled(true); ApplyEditHistory(true); }
    }

    inline void MainWindow::RootGrid_WorkflowKeyDown(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        bool const control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool const shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        auto const key = args.Key();
        auto const keyValue = static_cast<int32_t>(key);
        if (control && keyValue >= 0x31 && keyValue <= 0x36) { args.Handled(true); SelectFilter(keyValue - 0x31); return; }
        if (key == winrt::Windows::System::VirtualKey::PageUp) { args.Handled(true); MoveCurrentBy(-1); }
        else if (key == winrt::Windows::System::VirtualKey::PageDown) { args.Handled(true); MoveCurrentBy(1); }
        else if (control && key == winrt::Windows::System::VirtualKey::S)
        { args.Handled(true); if (shift) SaveAsFromShortcut(); else SaveFromShortcut(); }
        else if (control && key == winrt::Windows::System::VirtualKey::O) { args.Handled(true); OpenProjectFiles(); }
        else if (control && key == winrt::Windows::System::VirtualKey::F)
        { args.Handled(true); SearchTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic); SearchTextBox().SelectAll(); }
        else if (key == winrt::Windows::System::VirtualKey::F3) { args.Handled(true); MoveToSearchResult(shift ? -1 : 1); }
        else if (key == winrt::Windows::System::VirtualKey::F6) { args.Handled(true); MoveToReview(shift ? -1 : 1); }
        else if (key == winrt::Windows::System::VirtualKey::F7) { args.Handled(true); MoveToUntranslated(shift ? -1 : 1); }
        else if (key == winrt::Windows::System::VirtualKey::F8) { args.Handled(true); MoveToProblem(shift ? -1 : 1); }
    }

    inline void MainWindow::LoadFeatureState()
    {
        if (m_featureStateLoaded) return;
        m_featureStateLoaded = true;
        try
        {
            auto const* local = _wgetenv(L"LOCALAPPDATA");
            if (!local) return;
            auto const directory = std::filesystem::path{ local } / L"Aegisub" / L"TranslationWorkspace";
            std::filesystem::create_directories(directory);
            auto readFile = [](std::filesystem::path const& path) {
                std::ifstream stream(path, std::ios::binary);
                return std::string{ std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{} };
            };
            auto const settingsPath = directory / L"settings.tsv";
            if (std::filesystem::exists(settingsPath))
                agi::winui::ParseWorkspaceSettings(readFile(settingsPath), m_workspaceSettings);
            auto const recentPath = directory / L"recent-projects.tsv";
            if (std::filesystem::exists(recentPath))
                agi::winui::ParseRecentProjects(readFile(recentPath), m_recentProjects);
        }
        catch (...) {}

        QaCplBox().Value(static_cast<double>(m_workspaceSettings.qa.maximum_cpl));
        QaCpsBox().Value(m_workspaceSettings.qa.maximum_cps);
        QaMinDurationBox().Value(m_workspaceSettings.qa.minimum_duration);
        QaMaxLinesBox().Value(static_cast<double>(m_workspaceSettings.qa.maximum_lines));
        QaLengthMinBox().Value(m_workspaceSettings.qa.minimum_length_ratio);
        QaLengthMaxBox().Value(m_workspaceSettings.qa.maximum_length_ratio);
        CzechQuotesCheckBox().IsChecked(m_workspaceSettings.qa.check_czech_quotes);
        AutosaveCheckBox().IsChecked(m_workspaceSettings.autosave_draft);
        EditorFontSizeBox().Value(m_workspaceSettings.editor_font_size);
        TargetTextBox().FontSize(m_workspaceSettings.editor_font_size);
        OriginalTextBox().FontSize(m_workspaceSettings.editor_font_size);
    }

    inline void MainWindow::SaveFeatureSettings() const
    {
        try
        {
            auto const* local = _wgetenv(L"LOCALAPPDATA");
            if (!local) return;
            auto const directory = std::filesystem::path{ local } / L"Aegisub" / L"TranslationWorkspace";
            std::filesystem::create_directories(directory);
            std::ofstream stream(directory / L"settings.tsv", std::ios::binary | std::ios::trunc);
            stream << agi::winui::SerializeWorkspaceSettings(m_workspaceSettings);
        }
        catch (...) {}
    }

    inline void MainWindow::RememberRecentProject()
    {
        if (m_sourcePath.empty() || m_targetPath.empty()) return;
        agi::winui::TouchRecentProject(m_recentProjects,
            { winrt::to_string(m_sourcePath), winrt::to_string(m_targetPath) }, 10);
        try
        {
            auto const* local = _wgetenv(L"LOCALAPPDATA");
            if (!local) return;
            auto const directory = std::filesystem::path{ local } / L"Aegisub" / L"TranslationWorkspace";
            std::filesystem::create_directories(directory);
            std::ofstream stream(directory / L"recent-projects.tsv", std::ios::binary | std::ios::trunc);
            stream << agi::winui::SerializeRecentProjects(m_recentProjects);
        }
        catch (...) {}
    }

    inline void MainWindow::TargetTextBox_WorkflowLoaded(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_workflowHooksInstalled) return;
        m_workflowHooksInstalled = true;
        LoadFeatureState();
        TargetTextBox().PreviewKeyDown({ this, &MainWindow::TargetTextBox_WorkflowKeyDown });
        RootGrid().PreviewKeyDown({ this, &MainWindow::RootGrid_WorkflowKeyDown });
        TargetTextBox().TextChanged([this](auto const&, auto const&)
        {
            if (m_loadingSelection || !m_initialized || m_rows.empty()) return;
            auto& row = m_rows[m_currentIndex];
            if (row.status != L"Problém") row.workflowStatus = row.status;
            RefreshQaAll();
            RefreshCurrentQaVisuals();
        });
        PreviousButton().Click([this](auto const&, auto const&) { RefreshCurrentQaVisuals(); });
        NextButton().Click([this](auto const&, auto const&) { RefreshCurrentQaVisuals(); });
        InitializeWorkflowStatuses();
        RememberRecentProject();
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
            StatusBarText().Text(L"Zalomení beze změny · text je již vyvážený nebo nemá vhodnou mezeru");
            box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }
        auto const lineBreak = balanced.find(L"\r\n");
        box.Text(winrt::hstring{ balanced });
        box.SelectionStart(static_cast<int32_t>(lineBreak + 2));
        box.SelectionLength(0);
        box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    inline void MainWindow::MergeLinesButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        std::wstring input{ TargetTextBox().Text().c_str() };
        std::wstring output;
        output.reserve(input.size());
        bool spacePending = false;
        for (auto const c : input)
        {
            if (c == L'\r' || c == L'\n' || c == L'\t' || c == L' ')
            {
                spacePending = !output.empty();
                continue;
            }
            if (spacePending && !output.empty()) output.push_back(L' ');
            spacePending = false;
            output.push_back(c);
        }
        if (output == input) { StatusBarText().Text(L"Text už je na jednom řádku"); return; }
        TargetTextBox().Text(winrt::hstring{ output });
        TargetTextBox().SelectionStart(static_cast<int32_t>(output.size()));
        TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    inline void MainWindow::CopyOriginalButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_rows.empty()) return;
        auto const& original = m_rows[m_currentIndex].original;
        if (original.empty()) { StatusBarText().Text(L"Aktuální titulek nemá spárovaný originální text"); return; }
        auto const box = TargetTextBox();
        if (box.Text() == original) { StatusBarText().Text(L"Český text již odpovídá originálu"); return; }
        box.Text(original);
        box.SelectionStart(static_cast<int32_t>(original.size()));
        box.SelectionLength(0);
        box.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
        StatusBarText().Text(L"Originální text převzat · Ctrl+Z vrátí předchozí překlad");
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
        if (m_rows.empty()) return;
        auto& row = m_rows[m_currentIndex];
        auto& source = redo ? row.redoHistory : row.undoHistory;
        auto& destination = redo ? row.undoHistory : row.redoHistory;
        if (source.empty())
        {
            StatusBarText().Text(redo ? L"Není k dispozici žádná změna k opakování" : L"Není k dispozici žádná změna k vrácení");
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
        StatusBarText().Text(redo ? L"Změna zopakována · Ctrl+Y" : L"Změna vrácena · Ctrl+Z");
    }

    inline void MainWindow::UpdateDirtyFromRows()
    {
        SetDirty(m_workflowStateDirty || std::any_of(m_rows.begin(), m_rows.end(), [](auto const& row) { return row.targetModified; }));
        if (m_workspaceSettings.autosave_draft) ScheduleWorkspaceDraftSave();
    }

    inline void MainWindow::NextProblemButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) { MoveToProblem(1); }
    inline void MainWindow::NextUntranslatedButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) { MoveToUntranslated(1); }
    inline void MainWindow::NextReviewButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) { MoveToReview(1); }

    inline void MainWindow::MoveToReview(int32_t direction)
    {
        if (m_rows.empty() || direction == 0) return;
        auto const rowCount = static_cast<int32_t>(m_rows.size());
        for (int32_t offset = 1; offset <= rowCount; ++offset)
        {
            auto index = (m_currentIndex + direction * offset) % rowCount;
            if (index < 0) index += rowCount;
            auto const& row = m_rows[index];
            if (row.workflowStatus == L"Schváleno" && row.qaIssue.empty()) continue;
            if (index != m_currentIndex) { StoreCurrentEditorSelection(); m_currentIndex = index; LoadCurrentRow(); RefreshCurrentQaVisuals(); }
            StatusBarText().Text(winrt::hstring{ L"Titulek ke kontrole #" + std::to_wstring(row.number) + L" · F6/Shift+F6" });
            TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }
        StatusBarText().Text(L"Všechny titulky jsou schválené a bez problémů");
    }

    inline void MainWindow::MoveToUntranslated(int32_t direction)
    {
        if (m_rows.empty() || direction == 0) return;
        auto const rowCount = static_cast<int32_t>(m_rows.size());
        for (int32_t offset = 1; offset <= rowCount; ++offset)
        {
            auto index = (m_currentIndex + direction * offset) % rowCount;
            if (index < 0) index += rowCount;
            if (!IsTranslationEmpty(m_rows[index].target)) continue;
            if (index != m_currentIndex) { StoreCurrentEditorSelection(); m_currentIndex = index; LoadCurrentRow(); RefreshCurrentQaVisuals(); }
            StatusBarText().Text(winrt::hstring{ L"Nepřeložený titulek #" + std::to_wstring(m_rows[index].number) + L" · F7/Shift+F7" });
            TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }
        StatusBarText().Text(L"Všechny titulky obsahují překlad");
    }

    inline void MainWindow::MoveToProblem(int32_t direction)
    {
        if (m_rows.empty() || direction == 0) return;
        RefreshQaAll();
        auto const rowCount = static_cast<int32_t>(m_rows.size());
        for (int32_t offset = 1; offset <= rowCount; ++offset)
        {
            auto index = (m_currentIndex + direction * offset) % rowCount;
            if (index < 0) index += rowCount;
            if (m_rows[index].qaIssue.empty()) continue;
            StoreCurrentEditorSelection();
            m_currentIndex = index;
            LoadCurrentRow();
            RefreshCurrentQaVisuals();
            TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }
        StatusBarText().Text(L"QA · nebyl nalezen žádný problém");
    }

    inline agi::winui::SearchOptions MainWindow::CurrentSearchOptions() const
    {
        agi::winui::SearchOptions options;
        auto const scope = SearchScopeComboBox().SelectedIndex();
        options.scope = scope == 0 ? agi::winui::SearchScope::target
            : scope == 1 ? agi::winui::SearchScope::source : agi::winui::SearchScope::both;
        auto caseValue = SearchCaseCheckBox().IsChecked();
        auto wholeValue = SearchWholeWordCheckBox().IsChecked();
        options.case_sensitive = caseValue && caseValue.Value();
        options.whole_word = wholeValue && wholeValue.Value();
        return options;
    }

    inline void MainWindow::SearchOptions_Changed(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const options = CurrentSearchOptions();
        std::wstring const query{ SearchTextBox().Text().c_str() };
        size_t rows = 0, matches = 0;
        for (auto const index : VisibleRowIndices())
        {
            auto const& row = m_rows[index];
            auto countText = [&](std::wstring_view text) { return agi::winui::CountSearchMatches(text, query, options); };
            size_t count = 0;
            if (options.scope == agi::winui::SearchScope::target || options.scope == agi::winui::SearchScope::both)
                count += countText(std::wstring_view{ row.target.c_str(), row.target.size() });
            if (options.scope == agi::winui::SearchScope::source || options.scope == agi::winui::SearchScope::both)
                count += countText(std::wstring_view{ row.original.c_str(), row.original.size() });
            if (count) { ++rows; matches += count; }
        }
        SearchResultText().Text(winrt::hstring{ query.empty() ? L"" : std::to_wstring(matches) + L" / " + std::to_wstring(rows) + L" ř." });
    }

    inline void MainWindow::BulkNormalizeButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto indices = VisibleRowIndices();
        std::vector<size_t> affected;
        std::vector<std::wstring> normalized(m_rows.size());
        for (auto const index : indices)
        {
            std::wstring input{ m_rows[index].target.c_str() }, output;
            output.reserve(input.size());
            bool pendingSpace = false;
            for (size_t i = 0; i < input.size(); ++i)
            {
                auto const c = input[i];
                if (c == L'\r' || c == L'\n') { while (!output.empty() && output.back() == L' ') output.pop_back(); output += L"\r\n"; pendingSpace = false; if (c == L'\r' && i + 1 < input.size() && input[i + 1] == L'\n') ++i; continue; }
                if (std::iswspace(c)) { pendingSpace = !output.empty() && output.back() != L'\n'; continue; }
                if (pendingSpace && !agi::winui::IsSubtitlePunctuation(c)) output.push_back(L' ');
                pendingSpace = false;
                output.push_back(c);
            }
            while (!output.empty() && std::iswspace(output.back())) output.pop_back();
            normalized[index] = std::move(output);
            if (normalized[index] != input) affected.push_back(index);
        }
        if (affected.empty()) { StatusBarText().Text(L"Normalizace · ve zobrazených titulcích není co upravit"); return; }
        CaptureBulkSnapshot(affected, L"normalizace textu");
        for (auto const index : affected)
        {
            auto& row = m_rows[index];
            row.target = winrt::hstring{ normalized[index] };
            row.workflowStatus = L"Upraveno";
            row.targetModified = !agi::winui::EquivalentEditorText(row.target.c_str(), row.savedTarget.c_str());
            if (index < m_targetEntries.size()) m_targetEntries[index].text = row.target;
        }
        m_workflowStateDirty = true;
        UpdateDirtyFromRows();
        RefreshQaAll();
        LoadCurrentRow();
        StatusBarText().Text(winrt::hstring{ L"Normalizováno " + std::to_wstring(affected.size()) + L" titulků · lze vrátit" });
    }

    inline void MainWindow::ReloadProjectButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_sourcePath.empty() && m_targetPath.empty()) { StatusBarText().Text(L"Není načten žádný projekt"); return; }
        if (!ConfirmSaveBefore(L"znovunačtením projektu")) return;
        RefreshLoadedProject();
        RememberRecentProject();
        StatusBarText().Text(L"Projekt znovu načten z disku");
    }

    inline void MainWindow::ApplySettingsButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        m_workspaceSettings.qa.maximum_cpl = static_cast<size_t>((std::max)(1.0, QaCplBox().Value()));
        m_workspaceSettings.qa.maximum_cps = (std::max)(1.0, QaCpsBox().Value());
        m_workspaceSettings.qa.minimum_duration = (std::max)(0.0, QaMinDurationBox().Value());
        m_workspaceSettings.qa.maximum_lines = static_cast<size_t>((std::max)(1.0, QaMaxLinesBox().Value()));
        m_workspaceSettings.qa.minimum_length_ratio = (std::max)(0.05, QaLengthMinBox().Value());
        m_workspaceSettings.qa.maximum_length_ratio = (std::max)(m_workspaceSettings.qa.minimum_length_ratio, QaLengthMaxBox().Value());
        auto quotes = CzechQuotesCheckBox().IsChecked();
        auto autosave = AutosaveCheckBox().IsChecked();
        m_workspaceSettings.qa.check_czech_quotes = quotes && quotes.Value();
        m_workspaceSettings.autosave_draft = autosave && autosave.Value();
        m_workspaceSettings.editor_font_size = (std::min)(72.0, (std::max)(10.0, EditorFontSizeBox().Value()));
        TargetTextBox().FontSize(m_workspaceSettings.editor_font_size);
        OriginalTextBox().FontSize(m_workspaceSettings.editor_font_size);
        SaveFeatureSettings();
        RefreshQaAll();
        RefreshCurrentQaVisuals();
        StatusBarText().Text(L"Nastavení pracovního prostoru uloženo");
    }

    inline void MainWindow::RefreshPairingUi()
    {
        if (m_rows.empty()) return;
        auto const& row = m_rows[m_currentIndex];
        if (row.pairingIgnored) { PairingQualityText().Text(L"Pár: ignorováno"); return; }
        auto quality = row.manualSourceIndex >= 0 ? agi::winui::SubtitlePairQuality::excellent
            : agi::winui::AssessSubtitlePair(row.sourceMatchQuality, row.sourceStart.empty() ? 0U : 1U);
        PairingQualityText().Text(winrt::hstring{ L"Pár: " + agi::winui::SubtitlePairQualityLabel(quality) });
    }

    inline void MainWindow::ChangeManualPair(int32_t delta)
    {
        if (m_rows.empty() || m_sourceEntries.empty()) return;
        auto& row = m_rows[m_currentIndex];
        int32_t sourceIndex = row.manualSourceIndex >= 0 ? row.manualSourceIndex
            : (std::min)(m_currentIndex, static_cast<int32_t>(m_sourceEntries.size()) - 1);
        sourceIndex = (std::max)(0, (std::min)(sourceIndex + delta, static_cast<int32_t>(m_sourceEntries.size()) - 1));
        row.manualSourceIndex = sourceIndex;
        row.pairingIgnored = false;
        auto const& source = m_sourceEntries[static_cast<size_t>(sourceIndex)];
        row.original = source.text;
        row.sourceStart = source.start;
        row.sourceEnd = source.end;
        row.sourceMatchQuality = 1.0;
        m_workflowStateDirty = true;
        UpdateDirtyFromRows();
        RefreshQaAll();
        LoadCurrentRow();
        StatusBarText().Text(winrt::hstring{ L"Ruční pár nastaven na originál #" + std::to_wstring(sourceIndex + 1) });
    }

    inline void MainWindow::PairPreviousButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) { ChangeManualPair(-1); }
    inline void MainWindow::PairNextButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) { ChangeManualPair(1); }
    inline void MainWindow::PairIgnoreButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_rows.empty()) return;
        auto& row = m_rows[m_currentIndex];
        row.pairingIgnored = !row.pairingIgnored;
        m_workflowStateDirty = true;
        UpdateDirtyFromRows();
        RefreshQaAll();
        RefreshCurrentQaVisuals();
        StatusBarText().Text(row.pairingIgnored ? L"Časové párování tohoto řádku ignorováno" : L"Časové párování znovu zapnuto");
    }

    inline void MainWindow::RecentProjectsButton_Click(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        LoadFeatureState();
        if (m_recentProjects.empty())
        {
            MessageBoxW(GetActiveWindow(), L"Historie projektů je zatím prázdná.", L"Poslední projekty", MB_OK | MB_ICONINFORMATION);
            return;
        }
        std::wstring message;
        for (size_t i = 0; i < m_recentProjects.size(); ++i)
        {
            message += std::to_wstring(i + 1) + L". " + winrt::to_hstring(m_recentProjects[i].source_path).c_str() + L"\n   → "
                + winrt::to_hstring(m_recentProjects[i].target_path).c_str() + L"\n";
        }
        MessageBoxW(GetActiveWindow(), message.c_str(), L"Poslední projekty", MB_OK | MB_ICONINFORMATION);
    }
}
