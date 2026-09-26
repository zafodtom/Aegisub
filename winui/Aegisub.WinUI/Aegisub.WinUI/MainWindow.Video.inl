#pragma once

namespace winrt::Aegisub_WinUI::implementation
{
    inline std::wstring WinUiVideoFileUri(std::wstring path)
    {
        std::replace(path.begin(), path.end(), L'\\', L'/');
        std::wstring uri = L"file:///";
        uri.reserve(path.size() + 16);
        for (auto const c : path)
        {
            switch (c)
            {
            case L'%': uri += L"%25"; break;
            case L' ': uri += L"%20"; break;
            case L'#': uri += L"%23"; break;
            case L'?': uri += L"%3F"; break;
            default: uri.push_back(c); break;
            }
        }
        return uri;
    }

    inline bool MainWindow::OpenVideoFile(std::wstring const& filename)
    {
        if (filename.empty())
            return false;

        try
        {
            auto const absolute = std::filesystem::absolute(std::filesystem::path{ filename }).wstring();
            auto const mediaSource = winrt::Windows::Media::Core::MediaSource::CreateFromUri(
                winrt::Windows::Foundation::Uri{ WinUiVideoFileUri(absolute) });
            winrt::Windows::Media::Playback::MediaPlayer player;
            player.AutoPlay(false);
            player.Source(mediaSource);
            VideoPlayer().SetMediaPlayer(player);
            m_videoPath = absolute;
            VideoFileText().Text(winrt::hstring{ std::filesystem::path{ absolute }.filename().wstring() });
            SeekVideoToCurrentSubtitle();
            RefreshVideoPositionText();
            RenderWholeTimeline();
            RefreshTimelineSlider();
            LoadWaveformForMedia(absolute);
            StatusBarText().Text(L"Video načteno · výběr titulku sleduje čas videa");
            return true;
        }
        catch (winrt::hresult_error const& error)
        {
            MessageBoxW(GetActiveWindow(), error.message().c_str(), L"Video nelze otevřít", MB_OK | MB_ICONERROR);
            return false;
        }
    }

    inline double MainWindow::CurrentVideoSeconds()
    {
        if (m_videoPath.empty())
            return -1.0;
        try
        {
            auto const player = VideoPlayer().MediaPlayer();
            if (!player)
                return -1.0;
            return std::chrono::duration<double>(player.PlaybackSession().Position()).count();
        }
        catch (...)
        {
            return -1.0;
        }
    }

    inline void MainWindow::SeekVideoToCurrentSubtitle()
    {
        if (m_videoPath.empty() || m_rows.empty() || m_currentIndex < 0 ||
            m_currentIndex >= static_cast<int32_t>(m_rows.size()))
            return;

        try
        {
            auto const player = VideoPlayer().MediaPlayer();
            if (!player)
                return;
            player.Pause();
            m_playSelectedUntil = -1.0;
            VideoPlayPauseButton().Content(winrt::box_value(winrt::hstring{ L"▶" }));
            VideoPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"Přehrát titulek" }));
            auto const seconds = WorkflowTimestampSeconds(m_rows[m_currentIndex].start);
            auto const position = std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(
                std::chrono::duration<double>{ seconds });
            player.PlaybackSession().Position(position);
            RefreshVideoPositionText();
        }
        catch (...) {}
    }

    inline void MainWindow::OpenVideoButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        wchar_t buffer[32768]{};
        wchar_t const filter[] =
            L"Video (*.mp4;*.mkv;*.avi;*.mov;*.webm;*.m4v)\0*.mp4;*.mkv;*.avi;*.mov;*.webm;*.m4v\0"
            L"Všechny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Otevřít video";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog))
            OpenVideoFile(buffer);
    }

    inline void MainWindow::VideoSeekCurrentButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_videoPath.empty())
        {
            StatusBarText().Text(L"Nejprve otevřete video");
            return;
        }
        SeekVideoToCurrentSubtitle();
    }

    inline void MainWindow::VideoSetStartButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const seconds = CurrentVideoSeconds();
        if (seconds < 0.0)
        {
            StatusBarText().Text(L"Video není načtené");
            return;
        }
        StartTimeBox().Text(FormatWinUiTiming(seconds));
        ApplyCurrentTimingFromEditors();
    }

    inline void MainWindow::VideoSetEndButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const seconds = CurrentVideoSeconds();
        if (seconds < 0.0)
        {
            StatusBarText().Text(L"Video není načtené");
            return;
        }
        EndTimeBox().Text(FormatWinUiTiming(seconds));
        ApplyCurrentTimingFromEditors();
    }
    inline void MainWindow::VideoInfoButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_videoPath.empty())
        {
            StatusBarText().Text(L"Nejprve otevřete video");
            return;
        }

        try
        {
            auto const player = VideoPlayer().MediaPlayer();
            if (!player)
            {
                StatusBarText().Text(L"MediaPlayer není inicializovaný");
                return;
            }

            auto const session = player.PlaybackSession();
            auto const width = session.NaturalVideoWidth();
            auto const height = session.NaturalVideoHeight();
            if (width == 0 || height == 0)
            {
                StatusBarText().Text(L"Video stopa: 0×0 · zvuk může fungovat, ale Windows nedekóduje obrazový kodek");
                return;
            }

            StatusBarText().Text(winrt::hstring{
                L"Video stopa: " + std::to_wstring(width) + L"×" + std::to_wstring(height) });
        }
        catch (...)
        {
            StatusBarText().Text(L"Informaci o video stopě se nepodařilo načíst");
        }
    }
    inline void MainWindow::RefreshVideoPositionText()
    {
        auto const seconds = CurrentVideoSeconds();
        VideoPositionText().Text(seconds < 0.0 ? winrt::hstring{ L"00:00:00.000" } : FormatWinUiTiming(seconds));
    }

    inline void MainWindow::AdjustVideoPosition(double deltaSeconds)
    {
        auto const current = CurrentVideoSeconds();
        if (current < 0.0)
        {
            StatusBarText().Text(L"Nejprve otevřete video");
            return;
        }

        SeekMediaToSeconds((std::max)(0.0, current + deltaSeconds));
        RefreshVideoPositionText();
    }

    inline void MainWindow::VideoPlayPauseButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        m_playSelectedUntil = -1.0;
        VideoPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"Přehrát titulek" }));
        try
        {
            auto const player = VideoPlayer().MediaPlayer();
            if (!player)
            {
                StatusBarText().Text(L"Nejprve otevřete video");
                return;
            }

            if (player.PlaybackSession().PlaybackState() ==
                winrt::Windows::Media::Playback::MediaPlaybackState::Playing)
            {
                player.Pause();
                VideoPlayPauseButton().Content(winrt::box_value(winrt::hstring{ L"▶" }));
            }
            else
            {
                player.Play();
                VideoPlayPauseButton().Content(winrt::box_value(winrt::hstring{ L"❚❚" }));
            }
            RefreshVideoPositionText();
        }
        catch (...)
        {
            StatusBarText().Text(L"Přehrávání videa se nepodařilo změnit");
        }
    }

    inline void MainWindow::VideoBackFiveButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        AdjustVideoPosition(-5.0);
    }

    inline void MainWindow::VideoForwardFiveButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        AdjustVideoPosition(5.0);
    }

    inline void MainWindow::StartMediaUiTimer()
    {
        if (m_mediaUiTimer)
            return;

        winrt::Microsoft::UI::Xaml::DispatcherTimer timer;
        timer.Interval(std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(
            std::chrono::milliseconds{ 40 }));
        timer.Tick([this](auto const&, auto const&)
        {
            RefreshVideoPositionText();
            RefreshWaveformPlayhead();
            RefreshTimelineSlider();

            if (m_playSelectedUntil >= 0.0)
            {
                auto const current = CurrentVideoSeconds();
                if (current >= m_playSelectedUntil - 0.005)
                {
                    try
                    {
                        auto const player = VideoPlayer().MediaPlayer();
                        if (player)
                            player.Pause();
                    }
                    catch (...) {}
                    m_playSelectedUntil = -1.0;
                    VideoPlayPauseButton().Content(winrt::box_value(winrt::hstring{ L"▶" }));
                    VideoPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"Přehrát titulek" }));
                    RefreshWaveformPlayhead();
                }
            }
        });
        timer.Start();
        m_mediaUiTimer = timer;
    }

    inline void MainWindow::VideoPlaySelectedButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_videoPath.empty() || m_rows.empty())
        {
            StatusBarText().Text(L"Nejprve otevřete video a titulky");
            return;
        }

        try
        {
            auto const player = VideoPlayer().MediaPlayer();
            if (!player)
                return;

            if (m_playSelectedUntil >= 0.0 &&
                player.PlaybackSession().PlaybackState() ==
                    winrt::Windows::Media::Playback::MediaPlaybackState::Playing)
            {
                player.Pause();
                m_playSelectedUntil = -1.0;
                VideoPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"Přehrát titulek" }));
                VideoPlayPauseButton().Content(winrt::box_value(winrt::hstring{ L"▶" }));
                return;
            }

            auto const& row = m_rows[m_currentIndex];
            auto const start = WorkflowTimestampSeconds(row.start);
            auto const end = WorkflowTimestampSeconds(row.end);
            if (end <= start)
                return;

            SeekMediaToSeconds(start);
            m_playSelectedUntil = end;
            player.Play();
            VideoPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"Pozastavit" }));
            VideoPlayPauseButton().Content(winrt::box_value(winrt::hstring{ L"❚❚" }));
            RefreshWaveformPlayhead();
        }
        catch (...)
        {
            StatusBarText().Text(L"Vybraný titulek se nepodařilo přehrát");
        }
    }
}
