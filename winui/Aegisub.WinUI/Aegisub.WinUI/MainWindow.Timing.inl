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
        row.start = FormatWinUiTiming(start);
        row.end = FormatWinUiTiming(end);
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
}
