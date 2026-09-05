#pragma once

#include <filesystem>
#include <sstream>

namespace winrt::Aegisub_WinUI::implementation
{
    inline bool MainWindow::RowMatchesAdvancedSearch(SubtitleRowData const& row, std::wstring_view query) const
    {
        return agi::winui::SearchRowMatches(
            std::wstring_view{ row.original.c_str(), row.original.size() },
            std::wstring_view{ row.target.c_str(), row.target.size() },
            query,
            CurrentSearchOptions());
    }

    inline void MainWindow::RefreshAdvancedSearchSummary()
    {
        std::wstring const query{ SearchTextBox().Text().c_str() };
        if (query.empty())
        {
            SearchResultText().Text(L"");
            SearchPreviousButton().IsEnabled(false);
            SearchNextButton().IsEnabled(false);
            return;
        }

        auto const options = CurrentSearchOptions();
        size_t matchedRows = 0;
        size_t occurrences = 0;
        for (auto const index : VisibleRowIndices())
        {
            auto const& row = m_rows[index];
            size_t rowOccurrences = 0;
            if (options.scope == agi::winui::SearchScope::target || options.scope == agi::winui::SearchScope::both)
                rowOccurrences += agi::winui::CountSearchMatches(
                    std::wstring_view{ row.target.c_str(), row.target.size() }, query, options);
            if (options.scope == agi::winui::SearchScope::source || options.scope == agi::winui::SearchScope::both)
                rowOccurrences += agi::winui::CountSearchMatches(
                    std::wstring_view{ row.original.c_str(), row.original.size() }, query, options);
            if (rowOccurrences)
            {
                ++matchedRows;
                occurrences += rowOccurrences;
            }
        }
        bool const available = matchedRows > 0;
        SearchPreviousButton().IsEnabled(available);
        SearchNextButton().IsEnabled(available);
        SearchResultText().Text(winrt::hstring{
            std::to_wstring(occurrences) + L" / " + std::to_wstring(matchedRows) + L" ř." });
    }

    inline void MainWindow::MoveToAdvancedSearchResult(int32_t direction)
    {
        std::wstring const query{ SearchTextBox().Text().c_str() };
        if (query.empty() || m_rows.empty() || direction == 0)
        {
            SearchTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }

        auto const options = CurrentSearchOptions();
        auto const rowCount = static_cast<int32_t>(m_rows.size());
        for (int32_t offset = 1; offset <= rowCount; ++offset)
        {
            auto index = (m_currentIndex + direction * offset) % rowCount;
            if (index < 0) index += rowCount;
            if (!RowMatchesActiveFilter(m_rows[index]) || !RowMatchesAdvancedSearch(m_rows[index], query))
                continue;

            if (index != m_currentIndex)
            {
                StoreCurrentEditorSelection();
                m_currentIndex = index;
                LoadCurrentRow();
                RefreshCurrentQaVisuals();
            }

            if (options.scope != agi::winui::SearchScope::source)
            {
                auto const target = std::wstring_view{ m_rows[index].target.c_str(), m_rows[index].target.size() };
                auto const matches = agi::winui::FindSearchMatches(target, query, options);
                if (!matches.empty())
                {
                    TargetTextBox().SelectionStart(static_cast<int32_t>(matches.front().position));
                    TargetTextBox().SelectionLength(static_cast<int32_t>(matches.front().length));
                }
            }
            TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            StatusBarText().Text(winrt::hstring{
                L"Hledání „" + query + L"“ · titulek #" + std::to_wstring(m_rows[index].number) });
            return;
        }
        StatusBarText().Text(winrt::hstring{ L"Hledání „" + query + L"“ · žádná shoda v aktuálním filtru" });
    }

    inline void MainWindow::ApplyAdvancedReplace(bool previewOnly)
    {
        std::wstring const query{ SearchTextBox().Text().c_str() };
        std::wstring const replacement{ ReplaceTextBox().Text().c_str() };
        if (query.empty())
        {
            StatusBarText().Text(L"Nahradit · nejprve zadejte hledaný text");
            SearchTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }

        auto options = CurrentSearchOptions();
        if (options.scope == agi::winui::SearchScope::source)
        {
            StatusBarText().Text(L"Originál je pouze pro čtení · pro náhradu zvolte češtinu nebo obojí");
            return;
        }

        std::vector<size_t> affected;
        size_t occurrences = 0;
        for (auto const index : VisibleRowIndices())
        {
            auto const& row = m_rows[index];
            auto const count = agi::winui::CountSearchMatches(
                std::wstring_view{ row.target.c_str(), row.target.size() }, query, options);
            if (!count) continue;
            affected.push_back(index);
            occurrences += count;
        }

        if (affected.empty())
        {
            StatusBarText().Text(L"Nahradit · ve zobrazených českých titulcích není žádná shoda");
            return;
        }

        if (previewOnly)
        {
            std::wstring rows;
            auto const shown = (std::min)(affected.size(), size_t{ 10 });
            for (size_t i = 0; i < shown; ++i)
            {
                if (!rows.empty()) rows += L", ";
                rows += L"#" + std::to_wstring(m_rows[affected[i]].number);
            }
            if (affected.size() > shown) rows += L", …";
            StatusBarText().Text(winrt::hstring{
                L"Náhled náhrady · " + std::to_wstring(occurrences) + L" shod · " +
                std::to_wstring(affected.size()) + L" řádků · " + rows });
            return;
        }

        CaptureBulkSnapshot(affected, L"nahrazení textu");
        for (auto const index : affected)
        {
            auto& row = m_rows[index];
            auto const replaced = agi::winui::ReplaceSearchMatches(
                std::wstring_view{ row.target.c_str(), row.target.size() }, query, replacement, options);
            row.target = winrt::hstring{ replaced };
            row.workflowStatus = L"Upraveno";
            row.targetModified = !agi::winui::EquivalentEditorText(row.target.c_str(), row.savedTarget.c_str());
            if (index < m_targetEntries.size()) m_targetEntries[index].text = row.target;
        }
        m_workflowStateDirty = true;
        UpdateDirtyFromRows();
        RefreshQaAll();
        LoadCurrentRow();
        RefreshAdvancedSearchSummary();
        StatusBarText().Text(winrt::hstring{
            L"Nahrazeno " + std::to_wstring(occurrences) + L" shod v " +
            std::to_wstring(affected.size()) + L" titulcích · hromadnou změnu lze vrátit" });
    }

    inline void MainWindow::AdvancedSearchTextBox_TextChanged(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&)
    {
        RefreshAdvancedSearchSummary();
    }

    inline void MainWindow::AdvancedSearchTextBox_KeyDown(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        if (args.Key() != winrt::Windows::System::VirtualKey::Enter) return;
        bool const shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        args.Handled(true);
        MoveToAdvancedSearchResult(shift ? -1 : 1);
    }

    inline void MainWindow::AdvancedSearchPreviousButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        MoveToAdvancedSearchResult(-1);
    }

    inline void MainWindow::AdvancedSearchNextButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        MoveToAdvancedSearchResult(1);
    }

    inline void MainWindow::AdvancedReplacePreviewButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ApplyAdvancedReplace(true);
    }

    inline void MainWindow::AdvancedReplaceAllButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ApplyAdvancedReplace(false);
    }

    inline void MainWindow::SearchScopeComboBox_SelectionChanged(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        if (m_initialized) RefreshAdvancedSearchSummary();
    }

    inline void MainWindow::RefreshFeatureOverview()
    {
        std::vector<agi::winui::WorkflowRowState> states;
        states.reserve(m_rows.size());
        for (auto const& row : m_rows)
        {
            states.push_back({
                !IsTranslationEmpty(row.target),
                row.targetModified,
                row.workflowStatus == L"Připraveno",
                row.workflowStatus == L"Schváleno",
                !row.qaIssue.empty()
            });
        }
        auto const summary = agi::winui::SummarizeWorkflow(states);
        OverviewModifiedText().Text(winrt::hstring{ L"Upraveno: " + std::to_wstring(summary.modified) });
        OverviewReadyText().Text(winrt::hstring{ L"Připraveno bez QA problému: " + std::to_wstring(summary.ready) });
    }

    inline void MainWindow::RefreshRecentProjectsUi()
    {
        auto const combo = RecentProjectsComboBox();
        combo.Items().Clear();
        for (auto const& project : m_recentProjects)
        {
            auto const source = std::filesystem::path(winrt::to_hstring(project.source_path).c_str()).filename().wstring();
            auto const target = std::filesystem::path(winrt::to_hstring(project.target_path).c_str()).filename().wstring();
            combo.Items().Append(winrt::box_value(winrt::hstring{ source + L"  →  " + target }));
        }
        combo.IsEnabled(!m_recentProjects.empty());
        OpenSelectedRecentProjectButton().IsEnabled(!m_recentProjects.empty());
        if (!m_recentProjects.empty()) combo.SelectedIndex(0);
    }

    inline void MainWindow::OpenRecentProjectAt(size_t index)
    {
        if (index >= m_recentProjects.size()) return;
        auto const source = std::filesystem::path(winrt::to_hstring(m_recentProjects[index].source_path).c_str());
        auto const target = std::filesystem::path(winrt::to_hstring(m_recentProjects[index].target_path).c_str());
        if (!std::filesystem::exists(source) || !std::filesystem::exists(target) || source == target)
        {
            StatusBarText().Text(L"Vybraný poslední projekt už není na disku dostupný");
            return;
        }
        if (!ConfirmSaveBefore(L"otevřením vybraného posledního projektu")) return;
        std::vector<SubtitleEntry> sourceEntries;
        std::vector<SubtitleEntry> targetEntries;
        std::wstring error;
        if (!ReadSubtitleFile(source.wstring(), sourceEntries, error) ||
            !ReadSubtitleFile(target.wstring(), targetEntries, error))
        {
            MessageBoxW(GetActiveWindow(), error.c_str(), L"Poslední projekt nelze otevřít", MB_OK | MB_ICONERROR);
            return;
        }
        m_sourceEntries = std::move(sourceEntries);
        m_targetEntries = std::move(targetEntries);
        m_sourcePath = winrt::hstring{ source.wstring() };
        m_targetPath = winrt::hstring{ target.wstring() };
        RefreshLoadedProject();
        RememberRecentProject();
        RefreshRecentProjectsUi();
    }

    inline void MainWindow::OpenSelectedRecentProjectButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const index = RecentProjectsComboBox().SelectedIndex();
        if (index >= 0) OpenRecentProjectAt(static_cast<size_t>(index));
    }

    inline void MainWindow::RefreshRecoveryHistoryUi()
    {
        m_recoveryVersions.clear();
        auto const combo = RecoveryVersionsComboBox();
        combo.Items().Clear();
        if (m_targetPath.empty())
        {
            RecoveryHistoryText().Text(L"Recovery: otevřete český cílový soubor");
            RestoreSelectedRecoveryButton().IsEnabled(false);
            return;
        }
        try
        {
            auto const* local = _wgetenv(L"LOCALAPPDATA");
            if (!local) return;
            auto const directory = std::filesystem::path{ local } / L"Aegisub" / L"TranslationWorkspace" / L"Backups";
            auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
            std::vector<std::filesystem::directory_entry> entries;
            std::error_code error;
            std::filesystem::directory_iterator it(directory, error), end;
            while (!error && it != end)
            {
                auto const entry = *it;
                it.increment(error);
                if (!entry.is_regular_file()) continue;
                auto const name = entry.path().filename().wstring();
                if (entry.path().extension() == L".bak" && name.find(targetName) != std::wstring::npos)
                    entries.push_back(entry);
            }
            std::sort(entries.begin(), entries.end(), [](auto const& left, auto const& right) {
                return left.last_write_time() > right.last_write_time();
            });
            for (auto const& entry : entries)
            {
                m_recoveryVersions.push_back(entry.path().wstring());
                combo.Items().Append(winrt::box_value(winrt::hstring{ entry.path().filename().wstring() }));
            }
            if (!m_recoveryVersions.empty()) combo.SelectedIndex(0);
            RestoreSelectedRecoveryButton().IsEnabled(!m_recoveryVersions.empty());

            std::wstring draftStatus = L"žádný recovery draft";
            auto const workspace = std::filesystem::path{ local } / L"Aegisub" / L"TranslationWorkspace";
            std::filesystem::directory_iterator dit(workspace, error), dend;
            std::filesystem::file_time_type newest{};
            bool hasDraft = false;
            while (!error && dit != dend)
            {
                auto const entry = *dit;
                dit.increment(error);
                if (!entry.is_regular_file() || entry.path().extension() != L".draft") continue;
                auto const time = entry.last_write_time();
                if (!hasDraft || time > newest) { newest = time; hasDraft = true; }
            }
            if (hasDraft) draftStatus = L"recovery draft je uložen";
            RecoveryHistoryText().Text(winrt::hstring{
                L"Recovery: " + std::to_wstring(m_recoveryVersions.size()) + L" verzí · " + draftStatus });
        }
        catch (...)
        {
            RecoveryHistoryText().Text(L"Recovery historii se nepodařilo načíst");
            RestoreSelectedRecoveryButton().IsEnabled(false);
        }
    }

    inline void MainWindow::RestoreRecoveryVersion(size_t index)
    {
        if (index >= m_recoveryVersions.size() || m_targetPath.empty()) return;
        if (!ConfirmSaveBefore(L"obnovením historické verze")) return;
        try
        {
            CaptureRecoveryHistorySnapshot();
            std::filesystem::copy_file(
                std::filesystem::path{ m_recoveryVersions[index] },
                std::filesystem::path{ m_targetPath.c_str() },
                std::filesystem::copy_options::overwrite_existing);
            RefreshLoadedProject();
            RefreshRecoveryHistoryUi();
            StatusBarText().Text(L"Historická verze obnovena · předchozí aktuální soubor byl zazálohován");
        }
        catch (std::exception const& exception)
        {
            auto const message = winrt::to_hstring(exception.what());
            MessageBoxW(GetActiveWindow(), message.c_str(), L"Obnovení se nezdařilo", MB_OK | MB_ICONERROR);
        }
    }

    inline void MainWindow::RefreshRecoveryHistoryButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        RefreshRecoveryHistoryUi();
    }

    inline void MainWindow::RestoreSelectedRecoveryButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const index = RecoveryVersionsComboBox().SelectedIndex();
        if (index >= 0) RestoreRecoveryVersion(static_cast<size_t>(index));
    }

    inline void MainWindow::RootGrid_DragOver(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::DragEventArgs const& args)
    {
        auto const view = args.DataView();
        if (view.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
            args.AcceptedOperation(winrt::Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
    }

    inline void MainWindow::RootGrid_Drop(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::DragEventArgs const& args)
    {
        OpenDroppedProjectAsync(args);
    }

    inline winrt::fire_and_forget MainWindow::OpenDroppedProjectAsync(
        winrt::Microsoft::UI::Xaml::DragEventArgs args)
    {
        auto lifetime = get_strong();
        std::vector<std::wstring> files;
        try
        {
            auto const items = co_await args.DataView().GetStorageItemsAsync();
            for (auto const& item : items)
            {
                auto const file = item.try_as<winrt::Windows::Storage::StorageFile>();
                if (!file) continue;
                auto path = std::filesystem::path{ file.Path().c_str() };
                auto extension = path.extension().wstring();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
                if (extension == L".srt" || extension == L".ass" || extension == L".ssa")
                    files.push_back(path.wstring());
            }
        }
        catch (...) { co_return; }

        if (files.empty())
        {
            StatusBarText().Text(L"Přetažený obsah neobsahuje podporované titulky .srt/.ass/.ssa");
            co_return;
        }

        auto lower = [](std::wstring value) {
            std::transform(value.begin(), value.end(), value.begin(), ::towlower);
            return value;
        };
        auto isCzech = [&](std::wstring const& path) {
            auto const name = lower(std::filesystem::path(path).filename().wstring());
            return name.find(L".cs.") != std::wstring::npos || name.find(L"_cs.") != std::wstring::npos ||
                name.find(L"-cs.") != std::wstring::npos || name.find(L".cz.") != std::wstring::npos;
        };
        auto isEnglish = [&](std::wstring const& path) {
            auto const name = lower(std::filesystem::path(path).filename().wstring());
            return name.find(L".en.") != std::wstring::npos || name.find(L"_en.") != std::wstring::npos ||
                name.find(L"-en.") != std::wstring::npos;
        };

        std::wstring source;
        std::wstring target;
        if (files.size() >= 2)
        {
            for (auto const& file : files)
            {
                if (source.empty() && isEnglish(file)) source = file;
                if (target.empty() && isCzech(file)) target = file;
            }
            if (source.empty()) source = files[0];
            if (target.empty()) target = files[0] == source ? files[1] : files[0];
        }
        else
        {
            auto const only = files.front();
            auto path = std::filesystem::path{ only };
            auto name = path.filename().wstring();
            auto trySibling = [&](std::wstring from, std::wstring to) {
                auto lowered = lower(name);
                auto pos = lowered.find(from);
                if (pos == std::wstring::npos) return std::wstring{};
                auto siblingName = name;
                siblingName.replace(pos, from.size(), to);
                auto sibling = path.parent_path() / siblingName;
                return std::filesystem::exists(sibling) ? sibling.wstring() : std::wstring{};
            };
            if (isEnglish(only))
            {
                source = only;
                target = trySibling(L".en.", L".cs.");
                if (target.empty()) target = trySibling(L"_en.", L"_cs.");
                if (target.empty()) target = trySibling(L"-en.", L"-cs.");
            }
            else if (isCzech(only))
            {
                target = only;
                source = trySibling(L".cs.", L".en.");
                if (source.empty()) source = trySibling(L"_cs.", L"_en.");
                if (source.empty()) source = trySibling(L"-cs.", L"-en.");
            }
        }

        if (source.empty() || target.empty() || source == target)
        {
            StatusBarText().Text(L"Přetažení: dvojici originál + čeština se nepodařilo jednoznačně určit");
            co_return;
        }
        if (!ConfirmSaveBefore(L"otevřením přetaženého projektu")) co_return;

        std::vector<SubtitleEntry> sourceEntries;
        std::vector<SubtitleEntry> targetEntries;
        std::wstring error;
        if (!ReadSubtitleFile(source, sourceEntries, error) || !ReadSubtitleFile(target, targetEntries, error))
        {
            MessageBoxW(GetActiveWindow(), error.c_str(), L"Přetažený projekt nelze otevřít", MB_OK | MB_ICONERROR);
            co_return;
        }
        m_sourceEntries = std::move(sourceEntries);
        m_targetEntries = std::move(targetEntries);
        m_sourcePath = winrt::hstring{ source };
        m_targetPath = winrt::hstring{ target };
        RefreshLoadedProject();
        RememberRecentProject();
        RefreshRecentProjectsUi();
        StatusBarText().Text(L"Projekt otevřen přetažením");
    }
}
