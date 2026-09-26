#pragma once

namespace winrt::Aegisub_WinUI::implementation
{
    inline bool ParseWinUiTiming(winrt::hstring const& value, double& seconds)
    {
        try
        {
            std::wstring text{ value.c_str() };
            std::replace(text.begin(), text.end(), L',', L'.');
            auto const first = text.find(L':');
            auto const second = first == std::wstring::npos ? std::wstring::npos : text.find(L':', first + 1);
            if (first == std::wstring::npos || second == std::wstring::npos)
                return false;

            auto const hours = std::stoi(text.substr(0, first));
            auto const minutes = std::stoi(text.substr(first + 1, second - first - 1));
            auto const secs = std::stod(text.substr(second + 1));
            if (hours < 0 || minutes < 0 || minutes >= 60 || secs < 0.0 || secs >= 60.0)
                return false;

            seconds = hours * 3600.0 + minutes * 60.0 + secs;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    inline winrt::hstring FormatWinUiTiming(double seconds)
    {
        seconds = (std::max)(0.0, seconds);
        auto const totalMilliseconds = static_cast<long long>(seconds * 1000.0 + 0.5);
        auto const milliseconds = totalMilliseconds % 1000;
        auto const totalSeconds = totalMilliseconds / 1000;
        auto const secs = totalSeconds % 60;
        auto const totalMinutes = totalSeconds / 60;
        auto const minutes = totalMinutes % 60;
        auto const hours = totalMinutes / 60;

        std::wostringstream stream;
        stream << std::setfill(L'0')
               << std::setw(2) << hours << L':'
               << std::setw(2) << minutes << L':'
               << std::setw(2) << secs << L'.'
               << std::setw(3) << milliseconds;
        return winrt::hstring{ stream.str() };
    }

    inline void MainWindow::RefreshTimingEditor()
    {
        if (m_rows.empty() || m_currentIndex < 0 || m_currentIndex >= static_cast<int32_t>(m_rows.size()))
        {
            StartTimeBox().Text(L"");
            EndTimeBox().Text(L"");
            TimingDurationText().Text(L"");
            return;
        }

        auto const& row = m_rows[m_currentIndex];
        StartTimeBox().Text(row.start);
        EndTimeBox().Text(row.end);

        std::wostringstream duration;
        duration << std::fixed << std::setprecision(2) << row.duration << L" s";
        TimingDurationText().Text(winrt::hstring{ duration.str() });
    }

    inline bool MainWindow::ApplyCurrentTimingFromEditors()
    {
        if (m_rows.empty())
            return false;

        double start = 0.0;
        double end = 0.0;
        if (!ParseWinUiTiming(StartTimeBox().Text(), start) || !ParseWinUiTiming(EndTimeBox().Text(), end))
        {
            StatusBarText().Text(L"Čas musí mít formát HH:MM:SS.mmm");
            return false;
        }
        if (end <= start)
        {
            StatusBarText().Text(L"Konec titulku musí být později než začátek");
            return false;
        }

        auto& row = m_rows[m_currentIndex];
        auto const newStart = FormatWinUiTiming(start);
        auto const newEnd = FormatWinUiTiming(end);
        if (newStart != row.start || newEnd != row.end)
            CaptureWorkspaceUndoSnapshot(L"úprava časování");
        row.start = newStart;
        row.end = newEnd;
        row.duration = end - start;
        row.timingModified = row.start != row.savedStart || row.end != row.savedEnd;
        row.status = (row.targetModified || row.timingModified)
            ? winrt::hstring{ L"Upraveno" }
            : (row.savedWorkflowStatus.empty() ? winrt::hstring{ L"Uloženo" } : row.savedWorkflowStatus);

        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size()))
        {
            auto& entry = m_targetEntries[m_currentIndex];
            entry.start = row.start;
            entry.end = row.end;
            entry.startSeconds = start;
            entry.endSeconds = end;
            entry.duration = row.duration;
        }

        UpdateDirtyFromRows();
        RefreshQaAll();
        RebuildSubtitleGrid();
        LoadCurrentRow();
        RefreshCurrentQaVisuals();
        StatusBarText().Text(row.timingModified
            ? L"Časování upraveno · Ctrl+S uloží změnu do SRT"
            : L"Časování odpovídá uložené verzi");
        return true;
    }

    inline void MainWindow::AdjustCurrentTiming(double startDelta, double endDelta, winrt::hstring const& action)
    {
        if (m_rows.empty())
            return;

        auto& row = m_rows[m_currentIndex];
        double start = WorkflowTimestampSeconds(row.start) + startDelta;
        double end = WorkflowTimestampSeconds(row.end) + endDelta;

        start = (std::max)(0.0, start);
        end = (std::max)(0.0, end);
        if (end <= start + 0.001)
        {
            StatusBarText().Text(L"Úprava času by vytvořila neplatný interval");
            return;
        }

        StartTimeBox().Text(FormatWinUiTiming(start));
        EndTimeBox().Text(FormatWinUiTiming(end));
        if (ApplyCurrentTimingFromEditors())
            StatusBarText().Text(action);
    }

    inline void MainWindow::TimingApplyButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ApplyCurrentTimingFromEditors();
    }

    inline void MainWindow::TimingShiftBackButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        AdjustCurrentTiming(-0.1, -0.1, L"Titulek posunut o 100 ms zpět");
    }

    inline void MainWindow::TimingShiftForwardButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        AdjustCurrentTiming(0.1, 0.1, L"Titulek posunut o 100 ms vpřed");
    }

    inline void MainWindow::TimingStartBackButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        AdjustCurrentTiming(-0.1, 0.0, L"Začátek posunut o 100 ms zpět");
    }

    inline void MainWindow::TimingStartForwardButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        AdjustCurrentTiming(0.1, 0.0, L"Začátek posunut o 100 ms vpřed");
    }

    inline void MainWindow::TimingEndBackButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        AdjustCurrentTiming(0.0, -0.1, L"Konec posunut o 100 ms zpět");
    }

    inline void MainWindow::TimingEndForwardButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        AdjustCurrentTiming(0.0, 0.1, L"Konec posunut o 100 ms vpřed");
    }

    inline void MainWindow::TimingTextBox_KeyDown(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        if (args.Key() != winrt::Windows::System::VirtualKey::Enter)
            return;
        args.Handled(true);
        ApplyCurrentTimingFromEditors();
    }
    inline void MainWindow::ShiftAllSubtitles(double deltaSeconds)
    {
        if (m_rows.empty() || std::abs(deltaSeconds) < 0.0005)
            return;

        double earliestStart = WorkflowTimestampSeconds(m_rows.front().start);
        for (auto const& row : m_rows)
            earliestStart = (std::min)(earliestStart, WorkflowTimestampSeconds(row.start));

        // Preserve all relative timings. If a negative shift would cross 00:00,
        // reduce the requested shift rather than clamping rows individually.
        auto const effectiveDelta = (std::max)(deltaSeconds, -earliestStart);
        if (std::abs(effectiveDelta) < 0.0005)
            return;
        CaptureWorkspaceUndoSnapshot(L"posun všech titulků");

        for (auto& row : m_rows)
        {
            auto const start = WorkflowTimestampSeconds(row.start) + effectiveDelta;
            auto const end = WorkflowTimestampSeconds(row.end) + effectiveDelta;
            row.start = FormatWinUiTiming(start);
            row.end = FormatWinUiTiming(end);
            row.duration = (std::max)(0.0, end - start);
            row.timingModified = row.start != row.savedStart || row.end != row.savedEnd;
            if (row.timingModified)
            {
                row.workflowStatus = L"Upraveno";
                row.status = L"Upraveno";
            }
        }

        SyncTargetEntriesFromRows();
        ClearBulkUndo();
        UpdateDirtyFromRows();
        RefreshQaAll();
        RebuildSubtitleGrid();
        LoadCurrentRow();
        RefreshCurrentQaVisuals();
        RenderWaveform();
        ScheduleWorkspaceDraftSave();

        std::wostringstream message;
        message << L"Všechny titulky posunuty o "
                << std::fixed << std::setprecision(0)
                << effectiveDelta * 1000.0 << L" ms";
        if (std::abs(effectiveDelta - deltaSeconds) > 0.0005)
            message << L" · omezeno začátkem videa";
        StatusBarText().Text(winrt::hstring{ message.str() });
    }

    inline void MainWindow::InsertOneMillisecondSubtitleGaps()
    {
        if (m_rows.size() < 2)
            return;

        size_t changed = 0;
        bool historyCaptured = false;
        for (size_t index = 0; index + 1 < m_rows.size(); ++index)
        {
            auto& current = m_rows[index];
            auto const currentStartMs = static_cast<long long>(
                WorkflowTimestampSeconds(current.start) * 1000.0 + 0.5);
            auto const currentEndMs = static_cast<long long>(
                WorkflowTimestampSeconds(current.end) * 1000.0 + 0.5);
            auto const nextStartMs = static_cast<long long>(
                WorkflowTimestampSeconds(m_rows[index + 1].start) * 1000.0 + 0.5);

            if (currentEndMs != nextStartMs)
                continue;

            auto const newEndMs = nextStartMs - 1;
            if (newEndMs <= currentStartMs)
                continue;

            if (!historyCaptured)
            {
                CaptureWorkspaceUndoSnapshot(L"vložení 1 ms mezer");
                historyCaptured = true;
            }
            auto const newEnd = static_cast<double>(newEndMs) / 1000.0;
            current.end = FormatWinUiTiming(newEnd);
            current.duration = newEnd - static_cast<double>(currentStartMs) / 1000.0;
            current.timingModified = true;
            current.workflowStatus = L"Upraveno";
            current.status = L"Upraveno";
            ++changed;
        }

        if (changed == 0)
        {
            StatusBarText().Text(L"Nebyly nalezeny titulky s přesně navazujícím koncem a začátkem");
            return;
        }

        SyncTargetEntriesFromRows();
        ClearBulkUndo();
        UpdateDirtyFromRows();
        RefreshQaAll();
        RebuildSubtitleGrid();
        LoadCurrentRow();
        RefreshCurrentQaVisuals();
        RenderWaveform();
        ScheduleWorkspaceDraftSave();

        StatusBarText().Text(winrt::hstring{
            L"Vložena 1 ms mezera u " + std::to_wstring(changed) + L" navazujících titulků" });
    }

    inline winrt::fire_and_forget MainWindow::ShowShiftAllSubtitlesDialog()
    {
        auto lifetime = get_strong();

        winrt::Microsoft::UI::Xaml::Controls::ContentDialog dialog;
        dialog.XamlRoot(RootGrid().XamlRoot());
        dialog.Title(winrt::box_value(winrt::hstring{ L"Posunout všechny titulky" }));
        dialog.PrimaryButtonText(L"Použít");
        dialog.CloseButtonText(L"Zrušit");
        dialog.DefaultButton(winrt::Microsoft::UI::Xaml::Controls::ContentDialogButton::Primary);

        winrt::Microsoft::UI::Xaml::Controls::StackPanel panel;
        panel.Spacing(8.0);

        winrt::Microsoft::UI::Xaml::Controls::TextBlock description;
        description.Text(L"Zadejte posun v milisekundách. Záporná hodnota posune titulky zpět.");
        description.TextWrapping(winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
        panel.Children().Append(description);

        winrt::Microsoft::UI::Xaml::Controls::NumberBox offsetBox;
        offsetBox.Value(0.0);
        offsetBox.Minimum(-3600000.0);
        offsetBox.Maximum(3600000.0);
        offsetBox.SmallChange(10.0);
        offsetBox.LargeChange(100.0);
        panel.Children().Append(offsetBox);

        dialog.Content(panel);

        auto const result = co_await dialog.ShowAsync();
        if (result == winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
            ShiftAllSubtitles(offsetBox.Value() / 1000.0);
    }

    inline void MainWindow::ShiftAllSubtitlesMenuItem_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ShowShiftAllSubtitlesDialog();
    }

    inline void MainWindow::InsertSubtitleGapMenuItem_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        InsertOneMillisecondSubtitleGaps();
    }

}
