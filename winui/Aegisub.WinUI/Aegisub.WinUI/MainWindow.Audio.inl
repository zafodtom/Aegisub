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
        m_waveformViewportSubtitleIndex = m_currentIndex;
        CenterWaveformOnCurrentSubtitle();
        WaveformFileText().Text(winrt::hstring{
            std::filesystem::path{ filename }.filename().wstring() +
            L" · " + std::to_wstring(static_cast<int>(m_waveformDuration)) + L" s" });
        RenderWaveform();
        return true;
    }

    inline void MainWindow::CenterWaveformOnCurrentSubtitle()
    {
        if (m_waveformDuration <= 0.0 || m_rows.empty() || m_currentIndex < 0 ||
            m_currentIndex >= static_cast<int32_t>(m_rows.size()))
            return;

        auto const& row = m_rows[m_currentIndex];
        auto const activeStart = WorkflowTimestampSeconds(row.start);
        auto const activeEnd = WorkflowTimestampSeconds(row.end);
        auto const activeDuration = (std::max)(0.2, activeEnd - activeStart);
        auto const baseSpan = (std::max)(8.0, activeDuration + 5.0);
        auto const windowSpan = (std::max)(1.0, baseSpan / m_waveformHorizontalZoom);
        auto const centerTime = (activeStart + activeEnd) * 0.5;

        m_waveformWindowStart = (std::max)(0.0, centerTime - windowSpan * 0.5);
        m_waveformWindowEnd = (std::min)(m_waveformDuration, m_waveformWindowStart + windowSpan);
        if (m_waveformWindowEnd - m_waveformWindowStart < windowSpan &&
            m_waveformWindowEnd >= m_waveformDuration)
        {
            m_waveformWindowStart = (std::max)(0.0, m_waveformWindowEnd - windowSpan);
        }
    }

    inline void MainWindow::RefreshWaveformPlayhead()
    {
        if (!m_waveformPlayhead || m_waveformWindowEnd <= m_waveformWindowStart)
            return;

        auto const seconds = CurrentVideoSeconds();
        auto const width = WaveformCanvas().ActualWidth();
        if (seconds < m_waveformWindowStart || seconds > m_waveformWindowEnd || width <= 0.0)
        {
            m_waveformPlayhead.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            return;
        }

        auto const x = width * (seconds - m_waveformWindowStart) /
            (m_waveformWindowEnd - m_waveformWindowStart);
        m_waveformPlayhead.X1(x);
        m_waveformPlayhead.X2(x);
        m_waveformPlayhead.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
    }

    inline void MainWindow::RenderWaveform()
    {
        auto const canvas = WaveformCanvas();
        canvas.Children().Clear();
        m_waveformActiveSelection = nullptr;
        m_waveformActiveStartMarker = nullptr;
        m_waveformActiveEndMarker = nullptr;
        m_waveformPlayhead = nullptr;

        auto const width = canvas.ActualWidth();
        auto const height = canvas.ActualHeight();
        if (width < 10.0 || height < 10.0 || m_waveformPeaks.empty() || m_waveformDuration <= 0.0)
            return;

        if (m_waveformWindowEnd <= m_waveformWindowStart)
            CenterWaveformOnCurrentSubtitle();
        if (m_waveformWindowEnd <= m_waveformWindowStart)
            return;

        auto const visibleDuration = m_waveformWindowEnd - m_waveformWindowStart;
        auto const accent = TargetPanelBorder().BorderBrush();
        auto mapTimeToX = [&](double seconds) {
            return width * (seconds - m_waveformWindowStart) / visibleDuration;
        };

        // Show source subtitle ranges along the top and target ranges along the bottom.
        for (int32_t index = 0; index < static_cast<int32_t>(m_rows.size()); ++index)
        {
            auto const& subtitle = m_rows[index];

            auto drawRange = [&](double start, double end, double top, double barHeight, double opacity)
            {
                if (end < m_waveformWindowStart || start > m_waveformWindowEnd || end <= start)
                    return;
                auto const left = (std::max)(0.0, (std::min)(width, mapTimeToX((std::max)(start, m_waveformWindowStart))));
                auto const right = (std::max)(left, (std::min)(width, mapTimeToX((std::min)(end, m_waveformWindowEnd))));

                winrt::Microsoft::UI::Xaml::Shapes::Rectangle range;
                range.Fill(accent);
                range.Opacity(opacity);
                range.Width((std::max)(1.0, right - left));
                range.Height(barHeight);
                winrt::Microsoft::UI::Xaml::Controls::Canvas::SetLeft(range, left);
                winrt::Microsoft::UI::Xaml::Controls::Canvas::SetTop(range, top);
                canvas.Children().Append(range);
            };

            if (!subtitle.sourceStart.empty() && !subtitle.sourceEnd.empty())
            {
                drawRange(
                    WorkflowTimestampSeconds(subtitle.sourceStart),
                    WorkflowTimestampSeconds(subtitle.sourceEnd),
                    2.0,
                    index == m_currentIndex ? 6.0 : 4.0,
                    index == m_currentIndex ? 0.42 : 0.14);
            }

            drawRange(
                WorkflowTimestampSeconds(subtitle.start),
                WorkflowTimestampSeconds(subtitle.end),
                (std::max)(8.0, height - (index == m_currentIndex ? 13.0 : 9.0)),
                index == m_currentIndex ? 11.0 : 7.0,
                index == m_currentIndex ? 0.62 : 0.22);
        }

        auto const& active = m_rows[m_currentIndex];
        auto const activeStart = WorkflowTimestampSeconds(active.start);
        auto const activeEnd = WorkflowTimestampSeconds(active.end);

        // Highlight the editable target subtitle area without moving the viewport.
        if (activeEnd > activeStart)
        {
            auto const left = (std::max)(0.0, (std::min)(width, mapTimeToX(activeStart)));
            auto const right = (std::max)(left, (std::min)(width, mapTimeToX(activeEnd)));

            winrt::Microsoft::UI::Xaml::Shapes::Rectangle selection;
            selection.Fill(accent);
            selection.Opacity(0.10);
            selection.Width((std::max)(1.0, right - left));
            selection.Height(height);
            winrt::Microsoft::UI::Xaml::Controls::Canvas::SetLeft(selection, left);
            canvas.Children().Append(selection);
            m_waveformActiveSelection = selection;
        }

        auto const center = height * 0.5;
        auto const count = m_waveformPeaks.size();
        auto const columns = static_cast<size_t>((std::max)(1.0, width));
        auto timeToBin = [&](double seconds) {
            auto const normalized = (std::max)(0.0, (std::min)(1.0, seconds / m_waveformDuration));
            return (std::min)(count - 1,
                static_cast<size_t>(normalized * static_cast<double>(count - 1)));
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
            for (size_t sample = first; sample < last; ++sample)
            {
                minimum = (std::min)(minimum, m_waveformPeaks[sample].first);
                maximum = (std::max)(maximum, m_waveformPeaks[sample].second);
            }

            winrt::Microsoft::UI::Xaml::Shapes::Line peak;
            auto const x = static_cast<double>(column);
            peak.X1(x);
            peak.X2(x);
            peak.Y1((std::max)(0.0, (std::min)(height,
                center - static_cast<double>(maximum) * center * m_waveformVerticalGain)));
            peak.Y2((std::max)(0.0, (std::min)(height,
                center - static_cast<double>(minimum) * center * m_waveformVerticalGain)));
            peak.Stroke(accent);
            peak.StrokeThickness(1.0);
            peak.Opacity(0.78);
            canvas.Children().Append(peak);
        }

        auto drawBoundary = [&](double seconds, double thickness, double opacity)
            -> winrt::Microsoft::UI::Xaml::Shapes::Line
        {
            if (seconds < m_waveformWindowStart || seconds > m_waveformWindowEnd)
                return nullptr;
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
            return marker;
        };

        // Original timing is visible as subtle reference markers.
        if (!active.sourceStart.empty() && !active.sourceEnd.empty())
        {
            drawBoundary(WorkflowTimestampSeconds(active.sourceStart), 1.0, 0.28);
            drawBoundary(WorkflowTimestampSeconds(active.sourceEnd), 1.0, 0.28);
        }

        // Editable translated timing. These references are moved directly while dragging,
        // so the thousands of waveform lines do not need to be rebuilt on every mouse event.
        m_waveformActiveStartMarker = drawBoundary(activeStart, 2.5, 0.95);
        m_waveformActiveEndMarker = drawBoundary(activeEnd, 2.5, 0.95);

        winrt::Microsoft::UI::Xaml::Shapes::Line zero;
        zero.X1(0.0);
        zero.X2(width);
        zero.Y1(center);
        zero.Y2(center);
        zero.Stroke(accent);
        zero.StrokeThickness(1.0);
        zero.Opacity(0.20);
        canvas.Children().Append(zero);

        // Independent playback playhead; it moves while the waveform stays still.
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush playheadBrush;
        playheadBrush.Color(winrt::Windows::UI::Color{ 255, 210, 55, 45 });
        m_waveformPlayhead = winrt::Microsoft::UI::Xaml::Shapes::Line{};
        m_waveformPlayhead.Y1(0.0);
        m_waveformPlayhead.Y2(height);
        m_waveformPlayhead.Stroke(playheadBrush);
        m_waveformPlayhead.StrokeThickness(1.5);
        m_waveformPlayhead.Opacity(0.92);
        canvas.Children().Append(m_waveformPlayhead);
        RefreshWaveformPlayhead();

        std::wostringstream range;
        range << FormatWinUiTiming(m_waveformWindowStart).c_str()
              << L"  –  " << FormatWinUiTiming(m_waveformWindowEnd).c_str()
              << L"   |   překlad "
              << FormatWinUiTiming(activeStart).c_str()
              << L" – " << FormatWinUiTiming(activeEnd).c_str();
        WaveformRangeText().Text(winrt::hstring{ range.str() });
    }

    inline void MainWindow::RefreshWaveformTimingOverlay()
    {
        if (m_rows.empty() || m_waveformWindowEnd <= m_waveformWindowStart)
            return;

        auto const width = WaveformCanvas().ActualWidth();
        auto const height = WaveformCanvas().ActualHeight();
        if (width <= 0.0 || height <= 0.0)
            return;

        auto const& row = m_rows[m_currentIndex];
        auto const start = WorkflowTimestampSeconds(row.start);
        auto const end = WorkflowTimestampSeconds(row.end);
        auto const span = m_waveformWindowEnd - m_waveformWindowStart;
        auto const toX = [&](double seconds) {
            return width * (seconds - m_waveformWindowStart) / span;
        };

        auto const startX = toX(start);
        auto const endX = toX(end);

        if (m_waveformActiveStartMarker)
        {
            m_waveformActiveStartMarker.X1(startX);
            m_waveformActiveStartMarker.X2(startX);
        }
        if (m_waveformActiveEndMarker)
        {
            m_waveformActiveEndMarker.X1(endX);
            m_waveformActiveEndMarker.X2(endX);
        }
        if (m_waveformActiveSelection)
        {
            auto const left = (std::max)(0.0, (std::min)(width, startX));
            auto const right = (std::max)(left, (std::min)(width, endX));
            winrt::Microsoft::UI::Xaml::Controls::Canvas::SetLeft(m_waveformActiveSelection, left);
            m_waveformActiveSelection.Width((std::max)(1.0, right - left));
            m_waveformActiveSelection.Height(height);
        }

        std::wostringstream range;
        range << FormatWinUiTiming(m_waveformWindowStart).c_str()
              << L"  –  " << FormatWinUiTiming(m_waveformWindowEnd).c_str()
              << L"   |   překlad "
              << row.start.c_str() << L" – " << row.end.c_str();
        WaveformRangeText().Text(winrt::hstring{ range.str() });
    }

    inline void MainWindow::ZoomWaveformHorizontal(double factor)
    {
        if (m_waveformDuration <= 0.0 || factor <= 0.0)
            return;

        m_waveformHorizontalZoom = (std::max)(0.25,
            (std::min)(8.0, m_waveformHorizontalZoom * factor));

        auto const oldSpan = m_waveformWindowEnd > m_waveformWindowStart
            ? m_waveformWindowEnd - m_waveformWindowStart
            : (std::min)(m_waveformDuration, 8.0);
        auto const center = m_waveformWindowEnd > m_waveformWindowStart
            ? (m_waveformWindowStart + m_waveformWindowEnd) * 0.5
            : (m_rows.empty() ? 0.0 :
                (WorkflowTimestampSeconds(m_rows[m_currentIndex].start) +
                 WorkflowTimestampSeconds(m_rows[m_currentIndex].end)) * 0.5);

        auto const newSpan = (std::max)(0.75,
            (std::min)(m_waveformDuration, oldSpan / factor));
        m_waveformWindowStart = center - newSpan * 0.5;
        m_waveformWindowEnd = center + newSpan * 0.5;

        if (m_waveformWindowStart < 0.0)
        {
            m_waveformWindowEnd -= m_waveformWindowStart;
            m_waveformWindowStart = 0.0;
        }
        if (m_waveformWindowEnd > m_waveformDuration)
        {
            auto const overflow = m_waveformWindowEnd - m_waveformDuration;
            m_waveformWindowStart = (std::max)(0.0, m_waveformWindowStart - overflow);
            m_waveformWindowEnd = m_waveformDuration;
        }

        RenderWaveform();
    }

    inline void MainWindow::ZoomWaveformVertical(double factor)
    {
        if (factor <= 0.0)
            return;
        m_waveformVerticalGain = (std::max)(0.25,
            (std::min)(8.0, m_waveformVerticalGain * factor));
        RenderWaveform();
    }

    inline double MainWindow::WaveformSecondsFromPointer(double x, double width, bool allowAutoPan)
    {
        if (width <= 0.0 || m_waveformWindowEnd <= m_waveformWindowStart)
            return 0.0;

        auto const span = m_waveformWindowEnd - m_waveformWindowStart;
        if (allowAutoPan)
        {
            double shift = 0.0;
            if (x < 0.0)
                shift = -(std::max)(0.10, span * 0.045);
            else if (x > width)
                shift = (std::max)(0.10, span * 0.045);

            auto const now = GetTickCount64();
            if (shift != 0.0 && now - m_lastWaveformAutoPanTick >= 50)
            {
                m_lastWaveformAutoPanTick = now;
                auto newStart = m_waveformWindowStart + shift;
                auto newEnd = m_waveformWindowEnd + shift;
                if (newStart < 0.0)
                {
                    newEnd -= newStart;
                    newStart = 0.0;
                }
                if (newEnd > m_waveformDuration)
                {
                    auto const overflow = newEnd - m_waveformDuration;
                    newStart = (std::max)(0.0, newStart - overflow);
                    newEnd = m_waveformDuration;
                }
                m_waveformWindowStart = newStart;
                m_waveformWindowEnd = newEnd;
                RenderWaveform();
            }
        }

        auto const clampedX = (std::max)(0.0, (std::min)(width, x));
        return m_waveformWindowStart +
            clampedX / width * (m_waveformWindowEnd - m_waveformWindowStart);
    }

    inline void MainWindow::PreviewWaveformBoundary(double seconds)
    {
        if (m_rows.empty() || m_waveformDragMode == 0)
            return;

        auto& row = m_rows[m_currentIndex];
        auto start = WorkflowTimestampSeconds(row.start);
        auto end = WorkflowTimestampSeconds(row.end);

        if (m_waveformDragMode == 1)
            start = (std::max)(0.0, (std::min)(seconds, end - 0.01));
        else
            end = (std::max)(start + 0.01, seconds);

        row.start = FormatWinUiTiming(start);
        row.end = FormatWinUiTiming(end);
        row.duration = end - start;
        row.timingModified = row.start != row.savedStart || row.end != row.savedEnd;
        row.status = (row.targetModified || row.timingModified)
            ? winrt::hstring{ L"Upraveno" }
            : (row.savedWorkflowStatus.empty() ? winrt::hstring{ L"Uloženo" } : row.savedWorkflowStatus);

        RefreshTimingEditor();
        UpdateMetrics();
        RefreshWaveformTimingOverlay();
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

    inline void MainWindow::WaveformHorizontalZoomOutButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ZoomWaveformHorizontal(0.8);
    }

    inline void MainWindow::WaveformHorizontalZoomInButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ZoomWaveformHorizontal(1.25);
    }

    inline void MainWindow::WaveformVerticalZoomOutButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ZoomWaveformVertical(0.8);
    }

    inline void MainWindow::WaveformVerticalZoomInButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ZoomWaveformVertical(1.25);
    }

    inline void MainWindow::WaveformCanvas_PointerPressed(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (m_waveformDuration <= 0.0 || m_waveformPeaks.empty() || m_rows.empty())
            return;

        auto const point = args.GetCurrentPoint(WaveformCanvas());
        auto const properties = point.Properties();

        m_waveformDragMode = properties.IsRightButtonPressed() ? 2 :
            (properties.IsLeftButtonPressed() ? 1 : 0);
        if (m_waveformDragMode == 0)
            return;

        m_waveformDragActive = WaveformCanvas().CapturePointer(args.Pointer());
        m_lastWaveformAutoPanTick = 0;
        auto const seconds = WaveformSecondsFromPointer(
            point.Position().X, WaveformCanvas().ActualWidth(), false);
        PreviewWaveformBoundary(seconds);
        args.Handled(true);
    }

    inline void MainWindow::WaveformCanvas_PointerMoved(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!m_waveformDragActive || m_waveformDragMode == 0)
            return;

        auto const point = args.GetCurrentPoint(WaveformCanvas());
        auto const seconds = WaveformSecondsFromPointer(
            point.Position().X, WaveformCanvas().ActualWidth(), true);
        PreviewWaveformBoundary(seconds);
        args.Handled(true);
    }

    inline void MainWindow::WaveformCanvas_PointerReleased(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!m_waveformDragActive)
            return;

        WaveformCanvas().ReleasePointerCapture(args.Pointer());
        m_waveformDragActive = false;
        m_waveformDragMode = 0;

        // Full model/QA/list synchronization only once after the drag finishes.
        ApplyCurrentTimingFromEditors();
        RenderWaveform();
        args.Handled(true);
    }

}
