#pragma once

namespace winrt::Aegisub_WinUI::implementation
{
    inline void MainWindow::RenumberSubtitleRows()
    {
        for (size_t index = 0; index < m_rows.size(); ++index)
            m_rows[index].number = static_cast<int32_t>(index + 1);
    }

    inline void MainWindow::SyncTargetEntriesFromRows()
    {
        m_targetEntries.clear();
        m_targetEntries.reserve(m_rows.size());
        for (auto const& row : m_rows)
        {
            SubtitleEntry entry;
            entry.start = row.start;
            entry.end = row.end;
            entry.startSeconds = WorkflowTimestampSeconds(row.start);
            entry.endSeconds = WorkflowTimestampSeconds(row.end);
            entry.duration = (std::max)(0.0, entry.endSeconds - entry.startSeconds);
            entry.text = row.target;
            entry.rawText = row.targetModified ? row.target : row.rawTarget;
            m_targetEntries.push_back(std::move(entry));
        }
    }

    inline void MainWindow::RefreshAfterStructureEdit(winrt::hstring const& status)
    {
        if (m_rows.empty())
            m_currentIndex = 0;
        else
            m_currentIndex = (std::max)(0, (std::min)(
                m_currentIndex, static_cast<int32_t>(m_rows.size()) - 1));

        RenumberSubtitleRows();
        SyncTargetEntriesFromRows();
        m_structureDirty = true;
        ClearBulkUndo();
        UpdateDirtyFromRows();
        RefreshQaAll();
        RebuildSubtitleGrid();
        if (!m_rows.empty())
        {
            LoadCurrentRow();
            RefreshCurrentQaVisuals();
        }
        RefreshProgressSummary();
        RenderWaveform();
        StatusBarText().Text(status);
    }

    inline void MainWindow::SplitSubtitleButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_rows.empty())
            return;

        auto& current = m_rows[m_currentIndex];
        auto const text = std::wstring{ TargetTextBox().Text().c_str() };
        auto cursor = static_cast<size_t>((std::max)(0, TargetTextBox().SelectionStart()));
        cursor = (std::min)(cursor, text.size());
        if (cursor == 0 || cursor >= text.size())
        {
            StatusBarText().Text(L"Umístěte kurzor dovnitř českého textu, kde se má titulek rozdělit");
            return;
        }

        auto trim = [](std::wstring value) {
            while (!value.empty() && iswspace(value.front())) value.erase(value.begin());
            while (!value.empty() && iswspace(value.back())) value.pop_back();
            return value;
        };

        auto leftText = trim(text.substr(0, cursor));
        auto rightText = trim(text.substr(cursor));
        if (leftText.empty() || rightText.empty())
        {
            StatusBarText().Text(L"Obě části rozděleného titulku musí obsahovat text");
            return;
        }

        auto const start = WorkflowTimestampSeconds(current.start);
        auto const end = WorkflowTimestampSeconds(current.end);
        double split = CurrentVideoSeconds();
        if (split <= start + 0.05 || split >= end - 0.05)
            split = start + (end - start) * 0.5;

        SubtitleRowData second = current;
        second.start = FormatWinUiTiming(split);
        second.end = current.end;
        second.duration = end - split;
        second.target = winrt::hstring{ rightText };
        second.rawTarget = second.target;
        second.savedTarget = L"";
        second.savedStart = L"";
        second.savedEnd = L"";
        second.targetModified = true;
        second.timingModified = true;
        second.workflowStatus = L"Upraveno";
        second.status = L"Upraveno";
        second.undoHistory.clear();
        second.redoHistory.clear();
        second.selectionInitialized = false;

        current.end = FormatWinUiTiming(split);
        current.duration = split - start;
        current.target = winrt::hstring{ leftText };
        current.targetModified = true;
        current.timingModified = current.start != current.savedStart || current.end != current.savedEnd;
        current.workflowStatus = L"Upraveno";
        current.status = L"Upraveno";

        m_rows.insert(m_rows.begin() + m_currentIndex + 1, std::move(second));
        RefreshAfterStructureEdit(L"Titulek rozdělen v místě kurzoru");
    }

    inline void MainWindow::JoinNextSubtitleButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_rows.empty() || m_currentIndex >= static_cast<int32_t>(m_rows.size()) - 1)
        {
            StatusBarText().Text(L"Za aktuálním titulkem není další titulek ke spojení");
            return;
        }

        auto& current = m_rows[m_currentIndex];
        auto const& next = m_rows[m_currentIndex + 1];

        std::wstring joined{ current.target.c_str() };
        if (!joined.empty() && !next.target.empty())
            joined += L"\n";
        joined += next.target.c_str();

        if (!current.original.empty() && !next.original.empty())
            current.original = winrt::hstring{ std::wstring{ current.original.c_str() } + L"\n" + next.original.c_str() };
        else if (current.original.empty())
            current.original = next.original;

        current.target = winrt::hstring{ joined };
        current.rawTarget = current.target;
        current.end = next.end;
        current.duration = (std::max)(0.0,
            WorkflowTimestampSeconds(current.end) - WorkflowTimestampSeconds(current.start));
        current.targetModified = true;
        current.timingModified = current.start != current.savedStart || current.end != current.savedEnd;
        current.workflowStatus = L"Upraveno";
        current.status = L"Upraveno";

        m_rows.erase(m_rows.begin() + m_currentIndex + 1);
        RefreshAfterStructureEdit(L"Aktuální titulek spojen s následujícím");
    }

    inline void MainWindow::InsertSubtitleButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        double start = 0.0;
        if (!m_rows.empty())
            start = WorkflowTimestampSeconds(m_rows[m_currentIndex].end);

        double end = start + 2.0;
        if (!m_rows.empty() && m_currentIndex + 1 < static_cast<int32_t>(m_rows.size()))
        {
            auto const nextStart = WorkflowTimestampSeconds(m_rows[m_currentIndex + 1].start);
            if (nextStart > start + 0.1)
                end = nextStart;
        }

        SubtitleRowData row;
        row.start = FormatWinUiTiming(start);
        row.end = FormatWinUiTiming(end);
        row.duration = end - start;
        row.target = L"";
        row.rawTarget = L"";
        row.savedTarget = L"";
        row.savedStart = L"";
        row.savedEnd = L"";
        row.status = L"Upraveno";
        row.workflowStatus = L"Upraveno";
        row.targetModified = false;
        row.timingModified = true;
        row.historyInitialized = true;

        auto const position = m_rows.empty() ? 0 : m_currentIndex + 1;
        m_rows.insert(m_rows.begin() + position, std::move(row));
        m_currentIndex = position;
        RefreshAfterStructureEdit(L"Vložen nový prázdný titulek");
        TargetTextBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    inline void MainWindow::DeleteSubtitleButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_rows.empty())
            return;

        if (MessageBoxW(
            GetActiveWindow(),
            L"Opravdu smazat aktuální titulek?",
            L"Smazat titulek",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
            return;

        m_rows.erase(m_rows.begin() + m_currentIndex);
        if (m_currentIndex >= static_cast<int32_t>(m_rows.size()) && m_currentIndex > 0)
            --m_currentIndex;
        RefreshAfterStructureEdit(L"Titulek smazán");
    }
}
