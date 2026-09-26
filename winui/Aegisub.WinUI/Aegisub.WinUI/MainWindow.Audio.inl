#pragma once

#include <fstream>

namespace winrt::Aegisub_WinUI::implementation
{
    inline std::filesystem::path FindWaveformBridgeExecutable()
    {
        wchar_t modulePath[32768]{};
        auto const length = GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));
        if (!length || length >= std::size(modulePath))
            return {};

        auto path = std::filesystem::path{ modulePath }.parent_path() / L"aegisub-winui-bridge.exe";
        return std::filesystem::exists(path) ? path : std::filesystem::path{};
    }

    inline bool RunWaveformProcess(std::wstring commandLine, DWORD& exitCode)
    {
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};

        std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
        buffer.push_back(L'\0');

        if (!CreateProcessW(
            nullptr,
            buffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process))
            return false;

        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD result = 0;
        GetExitCodeProcess(process.hProcess, &result);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        exitCode = result;
        return true;
    }

    inline bool MainWindow::LoadWaveformForMedia(std::wstring const& filename)
    {
        auto const bridge = FindWaveformBridgeExecutable();
        if (bridge.empty() || filename.empty())
        {
            WaveformFileText().Text(L"waveform bridge není dostupný");
            return false;
        }

        auto output = std::filesystem::temp_directory_path() /
            (L"aegisub-winui-waveform-" + std::to_wstring(GetCurrentProcessId()) + L".tsv");

        std::error_code error;
        std::filesystem::remove(output, error);

        WaveformFileText().Text(L"načítám audio stopu…");
        std::wstring command =
            L"\"" + bridge.wstring() + L"\" --waveform \"" + filename +
            L"\" \"" + output.wstring() + L"\" 60000";

        DWORD exitCode = 0;
        if (!RunWaveformProcess(command, exitCode) || exitCode != 0)
        {
            WaveformFileText().Text(L"audio stopu se nepodařilo dekódovat");
            std::filesystem::remove(output, error);
            return false;
        }

        std::ifstream stream(output, std::ios::binary);
        if (!stream)
        {
            WaveformFileText().Text(L"waveform výstup chybí");
            return false;
        }

        std::string headerLine;
        if (!std::getline(stream, headerLine))
            return false;

        std::vector<std::string> header;
        size_t start = 0;
        while (start <= headerLine.size())
        {
            auto const end = headerLine.find('\t', start);
            header.push_back(headerLine.substr(start,
                end == std::string::npos ? std::string::npos : end - start));
            if (end == std::string::npos)
                break;
            start = end + 1;
        }

        if (header.size() < 5 || header[0] != "AEGISUB-WINUI-WAVEFORM" || header[1] != "1")
        {
            WaveformFileText().Text(L"neznámý waveform formát");
            return false;
        }

        try
        {
            m_waveformDuration = std::stod(header[2]);
        }
        catch (...)
        {
            m_waveformDuration = 0.0;
        }

        std::vector<std::pair<float, float>> peaks;
        std::string line;
        while (std::getline(stream, line))
        {
            auto const tab = line.find('\t');
            if (tab == std::string::npos)
                continue;
            try
            {
                peaks.emplace_back(
                    std::stof(line.substr(0, tab)),
                    std::stof(line.substr(tab + 1)));
            }
            catch (...) {}
        }

        std::filesystem::remove(output, error);
        if (peaks.empty() || m_waveformDuration <= 0.0)
        {
            WaveformFileText().Text(L"audio stopa je prázdná");
            return false;
        }

        m_waveformPeaks = std::move(peaks);
        m_waveformPath = filename;
        WaveformFileText().Text(winrt::hstring{
            std::filesystem::path{ filename }.filename().wstring() +
            L" · " + std::to_wstring(static_cast<int>(m_waveformDuration)) + L" s" });
        RenderWaveform();
        return true;
    }

    inline void MainWindow::RenderWaveform()
    {
        auto const canvas = WaveformCanvas();
        canvas.Children().Clear();

        auto const width = canvas.ActualWidth();
        auto const height = canvas.ActualHeight();
        if (width < 10.0 || height < 10.0 || m_waveformPeaks.empty() || m_waveformDuration <= 0.0)
            return;

        double activeStart = 0.0;
        double activeEnd = 0.0;
        if (!m_rows.empty() && m_currentIndex >= 0 && m_currentIndex < static_cast<int32_t>(m_rows.size()))
        {
            activeStart = WorkflowTimestampSeconds(m_rows[m_currentIndex].start);
            activeEnd = WorkflowTimestampSeconds(m_rows[m_currentIndex].end);
        }

        auto const activeDuration = (std::max)(0.2, activeEnd - activeStart);
        auto const windowSpan = (std::max)(8.0, activeDuration + 5.0);
        auto const centerTime = (activeStart + activeEnd) * 0.5;
        m_waveformWindowStart = (std::max)(0.0, centerTime - windowSpan * 0.5);
        m_waveformWindowEnd = (std::min)(m_waveformDuration, m_waveformWindowStart + windowSpan);
        if (m_waveformWindowEnd - m_waveformWindowStart < windowSpan && m_waveformWindowEnd >= m_waveformDuration)
            m_waveformWindowStart = (std::max)(0.0, m_waveformWindowEnd - windowSpan);

        auto const visibleDuration = (std::max)(0.001, m_waveformWindowEnd - m_waveformWindowStart);
        auto const accent = TargetPanelBorder().BorderBrush();
        auto mapTimeToX = [&](double seconds) {
            return width * (seconds - m_waveformWindowStart) / visibleDuration;
        };

        if (activeEnd > activeStart)
        {
            auto const left = (std::max)(0.0, (std::min)(width, mapTimeToX(activeStart)));
            auto const right = (std::max)(left, (std::min)(width, mapTimeToX(activeEnd)));

            winrt::Microsoft::UI::Xaml::Shapes::Rectangle selection;
            selection.Fill(accent);
            selection.Opacity(0.14);
            selection.Width((std::max)(1.0, right - left));
            selection.Height(height);
            winrt::Microsoft::UI::Xaml::Controls::Canvas::SetLeft(selection, left);
            canvas.Children().Append(selection);
        }

        auto const center = height * 0.5;
        auto const count = m_waveformPeaks.size();
        auto const columns = static_cast<size_t>((std::max)(1.0, width));

        auto timeToBin = [&](double seconds) {
            auto const normalized = (std::max)(0.0, (std::min)(1.0, seconds / m_waveformDuration));
            return (std::min)(count - 1, static_cast<size_t>(normalized * static_cast<double>(count - 1)));
        };

        for (size_t column = 0; column < columns; ++column)
        {
            auto const t0 = m_waveformWindowStart + visibleDuration *
                static_cast<double>(column) / static_cast<double>(columns);
            auto const t1 = m_waveformWindowStart + visibleDuration *
                static_cast<double>(column + 1) / static_cast<double>(columns);
            auto const first = timeToBin(t0);
            auto const last = (std::min)(count, timeToBin(t1) + 2);

            float minimum = 0.0f;
            float maximum = 0.0f;
            for (size_t index = first; index < last; ++index)
            {
                minimum = (std::min)(minimum, m_waveformPeaks[index].first);
                maximum = (std::max)(maximum, m_waveformPeaks[index].second);
            }

            winrt::Microsoft::UI::Xaml::Shapes::Line peak;
            auto const x = static_cast<double>(column);
            peak.X1(x);
            peak.X2(x);
            peak.Y1(center - static_cast<double>(maximum) * center);
            peak.Y2(center - static_cast<double>(minimum) * center);
            peak.Stroke(accent);
            peak.StrokeThickness(1.0);
            peak.Opacity(0.82);
            canvas.Children().Append(peak);
        }

        auto drawMarker = [&](double seconds, double thickness, double opacity)
        {
            if (seconds < m_waveformWindowStart || seconds > m_waveformWindowEnd)
                return;
            winrt::Microsoft::UI::Xaml::Shapes::Line marker;
            auto const x = mapTimeToX(seconds);
            marker.X1(x);
            marker.X2(x);
            marker.Y1(0.0);
            marker.Y2(height);
            marker.Stroke(accent);
            marker.StrokeThickness(thickness);
            marker.Opacity(opacity);
            canvas.Children().Append(marker);
        };

        drawMarker(activeStart, 2.0, 0.95);
        drawMarker(activeEnd, 2.0, 0.95);

        winrt::Microsoft::UI::Xaml::Shapes::Line zero;
        zero.X1(0.0);
        zero.X2(width);
        zero.Y1(center);
        zero.Y2(center);
        zero.Stroke(accent);
        zero.StrokeThickness(1.0);
        zero.Opacity(0.25);
        canvas.Children().Append(zero);

        std::wostringstream range;
        range << FormatWinUiTiming(m_waveformWindowStart).c_str()
              << L"  –  " << FormatWinUiTiming(m_waveformWindowEnd).c_str()
              << L"   |   titulek "
              << FormatWinUiTiming(activeStart).c_str()
              << L" – " << FormatWinUiTiming(activeEnd).c_str();
        WaveformRangeText().Text(winrt::hstring{ range.str() });
    }

    inline void MainWindow::SeekMediaToSeconds(double seconds)
    {
        try
        {
            auto const player = VideoPlayer().MediaPlayer();
            if (!player)
                return;

            seconds = (std::max)(0.0, seconds);
            auto const position = std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(
                std::chrono::duration<double>{ seconds });
            player.PlaybackSession().Position(position);
        }
        catch (...) {}
    }

    inline void MainWindow::OpenAudioButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        wchar_t buffer[32768]{};
        wchar_t const filter[] =
            L"Audio / video (*.wav;*.mp3;*.flac;*.aac;*.m4a;*.mp4;*.mkv;*.avi;*.mov;*.webm)\0*.wav;*.mp3;*.flac;*.aac;*.m4a;*.mp4;*.mkv;*.avi;*.mov;*.webm\0"
            L"Všechny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Otevřít audio nebo video pro waveform";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&dialog))
            return;

        auto const absolute = std::filesystem::absolute(std::filesystem::path{ buffer }).wstring();
        if (LoadWaveformForMedia(absolute))
        {
            try
            {
                auto const mediaSource = winrt::Windows::Media::Core::MediaSource::CreateFromUri(
                    winrt::Windows::Foundation::Uri{ WinUiVideoFileUri(absolute) });
                winrt::Windows::Media::Playback::MediaPlayer player;
                player.AutoPlay(false);
                player.Source(mediaSource);
                VideoPlayer().SetMediaPlayer(player);
                m_videoPath = absolute;
            }
            catch (...) {}
        }
    }

    inline void MainWindow::WaveformCanvas_SizeChanged(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const&)
    {
        RenderWaveform();
    }

    inline void MainWindow::WaveformCanvas_PointerPressed(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (m_waveformDuration <= 0.0 || m_waveformPeaks.empty())
            return;

        auto const width = WaveformCanvas().ActualWidth();
        if (width <= 0.0 || m_waveformWindowEnd <= m_waveformWindowStart)
            return;

        auto const point = args.GetCurrentPoint(WaveformCanvas()).Position();
        auto const ratio = (std::max)(0.0, (std::min)(1.0, point.X / width));
        auto const seconds = m_waveformWindowStart +
            ratio * (m_waveformWindowEnd - m_waveformWindowStart);

        SeekMediaToSeconds(seconds);
        args.Handled(true);
        StatusBarText().Text(winrt::hstring{
            std::wstring{ L"Pozice: " } + FormatWinUiTiming(seconds).c_str() +
            L" · použij Start = pozice / Konec = pozice pro nastavení času" });
    }

}
