#pragma once

#include <fstream>
#include <thread>

namespace winrt::SRTune::implementation
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

    inline std::filesystem::path FindWinUiFfmpegExecutable()
    {
        wchar_t modulePath[32768]{};
        auto const length = GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));
        if (length && length < std::size(modulePath))
        {
            auto const besideApplication = std::filesystem::path{ modulePath }.parent_path() / L"ffmpeg.exe";
            if (std::filesystem::exists(besideApplication))
                return besideApplication;
        }

        wchar_t searchResult[32768]{};
        auto const found = SearchPathW(nullptr, L"ffmpeg.exe", nullptr,
            static_cast<DWORD>(std::size(searchResult)), searchResult, nullptr);
        if (found && found < std::size(searchResult))
            return std::filesystem::path{ searchResult };

        return {};
    }

    inline std::wstring SrtTimestampFromWinUi(winrt::hstring const& value)
    {
        std::wstring result{ value.c_str() };
        if (result.size() > 8 && result[8] == L'.')
            result[8] = L',';
        return result;
    }

    template<typename Rows>
    inline bool WriteWinUiBurnSrt(
        std::filesystem::path const& output,
        Rows const& rows)
    {
        std::ofstream stream(output, std::ios::binary | std::ios::trunc);
        if (!stream)
            return false;

        size_t number = 0;
        for (auto const& row : rows)
        {
            if (row.target.empty())
                continue;

            auto text = winrt::to_string(row.target);
            std::string normalized;
            normalized.reserve(text.size());
            for (size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] == '\r')
                {
                    if (i + 1 < text.size() && text[i + 1] == '\n')
                        continue;
                    normalized.push_back('\n');
                }
                else
                {
                    normalized.push_back(text[i]);
                }
            }

            ++number;
            stream << number << "\r\n"
                   << winrt::to_string(winrt::hstring{ SrtTimestampFromWinUi(row.start) })
                   << " --> "
                   << winrt::to_string(winrt::hstring{ SrtTimestampFromWinUi(row.end) })
                   << "\r\n";
            for (char ch : normalized)
            {
                if (ch == '\n') stream << "\r\n";
                else stream.put(ch);
            }
            stream << "\r\n\r\n";
        }

        return number > 0 && stream.good();
    }

    inline bool RunWinUiFfmpegBurn(
        std::filesystem::path const& ffmpeg,
        std::filesystem::path const& inputVideo,
        std::filesystem::path const& workingDirectory,
        std::filesystem::path const& outputVideo,
        DWORD& exitCode)
    {
        std::wstring command =
            L"\"" + ffmpeg.wstring() +
            L"\" -y -hide_banner -loglevel warning -i \"" + inputVideo.wstring() +
            L"\" -map 0:v:0 -map 0:a? -vf \"subtitles=subtitles.srt:charenc=UTF-8\""
            L" -c:v libx264 -preset medium -crf 20 -c:a aac -b:a 192k -movflags +faststart \"" +
            outputVideo.wstring() + L"\"";

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        std::vector<wchar_t> buffer(command.begin(), command.end());
        buffer.push_back(L'\0');

        if (!CreateProcessW(
            nullptr,
            buffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDirectory.c_str(),
            &startup,
            &process))
        {
            exitCode = GetLastError();
            return false;
        }

        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD result = 1;
        GetExitCodeProcess(process.hProcess, &result);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        exitCode = result;
        return true;
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

    inline void MainWindow::CloseVideoFile()
    {
        try
        {
            auto const player = VideoPlayer().MediaPlayer();
            if (player)
                player.Pause();
            VideoPlayer().SetMediaPlayer(nullptr);
        }
        catch (...) {}

        m_videoPath.clear();
        m_playSelectedUntil = -1.0;
        VideoFileText().Text(L"");
        VideoPositionText().Text(L"");
        VideoPlayPauseButton().Content(winrt::box_value(winrt::hstring{ L"▶" }));
        VideoPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"Přehrát titulek" }));
        WaveformPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"▶ Titulek" }));
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
            WaveformPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"▶ Titulek" }));
            auto const seconds = WorkflowTimestampSeconds(m_rows[m_currentIndex].start);
            auto const position = std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(
                std::chrono::duration<double>{ seconds });
            player.PlaybackSession().Position(position);
            RefreshVideoPositionText();
            RefreshTimelineSlider();
            RefreshWaveformPlayhead();
        }
        catch (...) {}
    }

    inline void MainWindow::BurnSubtitlesMenuItem_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_videoBurnInProgress)
        {
            StatusBarText().Text(L"Vypalování videa už probíhá");
            return;
        }
        if (m_videoPath.empty())
        {
            MessageBoxW(GetActiveWindow(), L"Nejprve otevřete video.", L"Vypálit překlad do videa", MB_OK | MB_ICONINFORMATION);
            return;
        }
        if (m_rows.empty() || std::none_of(m_rows.begin(), m_rows.end(), [](auto const& row) { return !row.target.empty(); }))
        {
            MessageBoxW(GetActiveWindow(), L"Překlad neobsahuje žádný text k vypálení.", L"Vypálit překlad do videa", MB_OK | MB_ICONINFORMATION);
            return;
        }

        auto const ffmpeg = FindWinUiFfmpegExecutable();
        if (ffmpeg.empty())
        {
            MessageBoxW(GetActiveWindow(),
                L"FFmpeg nebyl nalezen. Umístěte ffmpeg.exe vedle aplikace nebo jej přidejte do systémové proměnné PATH.",
                L"FFmpeg není dostupný", MB_OK | MB_ICONERROR);
            return;
        }

        std::filesystem::path const inputPath{ m_videoPath };
        auto suggested = inputPath.parent_path() / (inputPath.stem().wstring() + L"_titulky.mp4");
        wchar_t buffer[32768]{};
        wcsncpy_s(buffer, suggested.wstring().c_str(), _TRUNCATE);
        wchar_t const filter[] = L"MP4 video (*.mp4)\0*.mp4\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Uložit video s vypáleným překladem";
        dialog.lpstrDefExt = L"mp4";
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&dialog))
            return;

        auto const outputPath = std::filesystem::absolute(std::filesystem::path{ buffer });
        std::error_code equivalentError;
        if (std::filesystem::equivalent(inputPath, outputPath, equivalentError) && !equivalentError)
        {
            MessageBoxW(GetActiveWindow(), L"Výstupní video nesmí přepsat právě otevřené zdrojové video.",
                L"Neplatný výstup", MB_OK | MB_ICONWARNING);
            return;
        }

        StartBurnSubtitlesToVideo(outputPath.wstring());
    }

    inline void MainWindow::StartBurnSubtitlesToVideo(std::wstring const& outputPath)
    {
        if (m_videoBurnInProgress || m_videoPath.empty())
            return;

        if (m_currentIndex >= 0 && m_currentIndex < static_cast<int32_t>(m_rows.size()))
            m_rows[m_currentIndex].target = TargetTextBox().Text();

        auto const ffmpeg = FindWinUiFfmpegExecutable();
        if (ffmpeg.empty())
            return;

        auto const inputVideo = std::filesystem::path{ m_videoPath };
        auto const outputVideo = std::filesystem::path{ outputPath };
        auto const rows = m_rows;
        auto const temporaryDirectory = std::filesystem::temp_directory_path() /
            (L"srtune-burn-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));

        std::error_code error;
        std::filesystem::create_directories(temporaryDirectory, error);
        if (error || !WriteWinUiBurnSrt(temporaryDirectory / L"subtitles.srt", rows))
        {
            MessageBoxW(GetActiveWindow(), L"Nepodařilo se vytvořit dočasný soubor titulků.",
                L"Vypalování selhalo", MB_OK | MB_ICONERROR);
            std::filesystem::remove_all(temporaryDirectory, error);
            return;
        }

        m_videoBurnInProgress = true;
        BurnSubtitlesMenuItem().IsEnabled(false);
        StatusBarText().Text(L"Vypaluji překlad do videa…");

        auto lifetime = get_strong();
        auto dispatcher = DispatcherQueue();
        std::thread([lifetime, dispatcher, ffmpeg, inputVideo, outputVideo, temporaryDirectory]()
        {
            DWORD exitCode = 1;
            bool const started = RunWinUiFfmpegBurn(ffmpeg, inputVideo, temporaryDirectory, outputVideo, exitCode);
            std::error_code cleanupError;
            std::filesystem::remove_all(temporaryDirectory, cleanupError);

            dispatcher.TryEnqueue([lifetime, started, exitCode, outputVideo]()
            {
                lifetime->m_videoBurnInProgress = false;
                lifetime->BurnSubtitlesMenuItem().IsEnabled(true);
                if (started && exitCode == 0)
                {
                    lifetime->StatusBarText().Text(winrt::hstring{
                        L"Video s vypáleným překladem vytvořeno · " + outputVideo.filename().wstring() });
                    MessageBoxW(GetActiveWindow(), outputVideo.wstring().c_str(),
                        L"Video bylo vytvořeno", MB_OK | MB_ICONINFORMATION);
                }
                else
                {
                    std::wstring message = started
                        ? L"FFmpeg export selhal (kód " + std::to_wstring(exitCode) +
                            L"). Ověřte, že použitý FFmpeg podporuje filtr subtitles/libass a enkodér libx264."
                        : L"FFmpeg se nepodařilo spustit.";
                    lifetime->StatusBarText().Text(L"Vypalování videa selhalo");
                    MessageBoxW(GetActiveWindow(), message.c_str(), L"Vypalování selhalo", MB_OK | MB_ICONERROR);
                }
            });
        }).detach();
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
        if (seconds < 0.0)
        {
            VideoPositionText().Text(L"00:00:00.000");
            return;
        }

        auto const timeText = FormatWinUiTiming(seconds);
        auto const playingIndex = FindSubtitleIndexForTime(seconds);

        std::wstring videoLabel;
        if (playingIndex >= 0 && playingIndex < static_cast<int32_t>(m_rows.size()))
            videoLabel = L"#" + std::to_wstring(m_rows[playingIndex].number) + L" · ";
        videoLabel += timeText.c_str();
        VideoPositionText().Text(winrt::hstring{ videoLabel });

        if (!m_rows.empty() && m_currentIndex >= 0 &&
            m_currentIndex < static_cast<int32_t>(m_rows.size()))
        {
            auto const& row = m_rows[m_currentIndex];
            TablePositionText().Text(winrt::hstring{
                L"#" + std::to_wstring(row.number) + L" / " + std::to_wstring(m_rows.size()) +
                L" · " + std::wstring{ row.start.c_str() } + L" → " + std::wstring{ row.end.c_str() } +
                L" · video " + std::wstring{ timeText.c_str() } });
        }
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
            WaveformPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"▶ Titulek" }));
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
            std::chrono::milliseconds{ 50 }));
        timer.Tick([this](auto const&, auto const&)
        {
            try
            {
                auto const player = VideoPlayer().MediaPlayer();
                if (!player ||
                    player.PlaybackSession().PlaybackState() !=
                        winrt::Windows::Media::Playback::MediaPlaybackState::Playing)
                {
                    return;
                }

                auto const playbackSeconds = CurrentVideoSeconds();
                if (playbackSeconds < 0.0)
                    return;

                // Lightweight moving line can update often.
                RefreshWaveformPlayhead();

                // Text and the whole-video slider do not need frame-rate updates.
                auto const now = GetTickCount64();
                if (now - m_lastTimelineUiTick >= 125)
                {
                    m_lastTimelineUiTick = now;
                    RefreshVideoPositionText();
                    RefreshTimelineSlider();
                }

                SyncSubtitleToPlayback(playbackSeconds);
                FollowWaveformPlayback(playbackSeconds);

                if (m_playSelectedUntil >= 0.0 &&
                    playbackSeconds >= m_playSelectedUntil - 0.005)
                {
                    player.Pause();
                    m_playSelectedUntil = -1.0;
                    VideoPlayPauseButton().Content(winrt::box_value(winrt::hstring{ L"▶" }));
                    VideoPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"Přehrát titulek" }));
            WaveformPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"▶ Titulek" }));
                    RefreshVideoPositionText();
                    RefreshTimelineSlider();
                    RefreshWaveformPlayhead();
                }
            }
            catch (...) {}
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
            WaveformPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"▶ Titulek" }));
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
            WaveformPlaySelectedButton().Content(winrt::box_value(winrt::hstring{ L"❚❚ Titulek" }));
            VideoPlayPauseButton().Content(winrt::box_value(winrt::hstring{ L"❚❚" }));
            RefreshWaveformPlayhead();
        }
        catch (...)
        {
            StatusBarText().Text(L"Vybraný titulek se nepodařilo přehrát");
        }
    }
}
