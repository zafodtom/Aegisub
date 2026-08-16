#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "Comdlg32.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace
{
    std::filesystem::path FindBridgeFrom(std::filesystem::path start)
    {
        if (start.empty())
        {
            return {};
        }

        if (!std::filesystem::is_directory(start))
        {
            start = start.parent_path();
        }

        for (int depth = 0; depth < 12 && !start.empty(); ++depth)
        {
            auto const candidate = start / L"build" / L"src" / L"aegisub-winui-bridge.exe";
            if (std::filesystem::exists(candidate))
            {
                return candidate;
            }

            auto const parent = start.parent_path();
            if (parent == start)
            {
                break;
            }
            start = parent;
        }

        return {};
    }

    std::filesystem::path FindBridgeExecutable()
    {
        wchar_t modulePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath))) != 0)
        {
            if (auto const bridge = FindBridgeFrom(std::filesystem::path(modulePath)); !bridge.empty())
            {
                return bridge;
            }
        }

        std::error_code error;
        auto const current = std::filesystem::current_path(error);
        if (!error)
        {
            return FindBridgeFrom(current);
        }

        return {};
    }

    std::string UnescapeBridgeField(std::string const& value)
    {
        std::string output;
        output.reserve(value.size());

        for (size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] != '\\' || i + 1 >= value.size())
            {
                output.push_back(value[i]);
                continue;
            }

            switch (value[++i])
            {
            case '\\': output.push_back('\\'); break;
            case 't': output.push_back('\t'); break;
            case 'r': output.push_back('\r'); break;
            case 'n': output.push_back('\n'); break;
            default:
                output.push_back('\\');
                output.push_back(value[i]);
                break;
            }
        }

        return output;
    }

    double TimestampSeconds(std::string const& value)
    {
        if (value.size() < 12)
        {
            return 0.0;
        }

        try
        {
            auto const hours = std::stoi(value.substr(0, 2));
            auto const minutes = std::stoi(value.substr(3, 2));
            auto const seconds = std::stod(value.substr(6));
            return hours * 3600.0 + minutes * 60.0 + seconds;
        }
        catch (...)
        {
            return 0.0;
        }
    }

    std::wstring ToWide(std::string const& value)
    {
        auto const converted = winrt::to_hstring(value);
        return std::wstring(converted.c_str());
    }
}

namespace winrt::Aegisub_WinUI::implementation
{
    int32_t MainWindow::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MainWindow::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void MainWindow::RootGrid_Loaded(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_initialized)
        {
            return;
        }

        m_initialized = true;
        InitializeDynamicSubtitleGrid();
        RebuildSubtitleGrid();
        HookOpenProjectButton();
        LoadCurrentRow();
    }

    void MainWindow::PreviousButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_rows.empty() || m_currentIndex <= 0)
        {
            return;
        }

        --m_currentIndex;
        LoadCurrentRow();
    }

    void MainWindow::NextButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_rows.empty() || m_currentIndex >= static_cast<int32_t>(m_rows.size()) - 1)
        {
            return;
        }

        ++m_currentIndex;
        LoadCurrentRow();
    }

    void MainWindow::ApproveButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_rows.empty())
        {
            return;
        }

        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        row.status = L"Schv\u00E1leno";

        UpdateTableRow(m_currentIndex);
        TargetInfoText().Text(hstring{ L"#" + std::to_wstring(row.number) + L" \u00B7 schv\u00E1leno" });
        TargetStatusText().Text(L"Stav: schv\u00E1leno");
        StatusBarText().Text(L"Titulek schv\u00E1len");

        if (m_currentIndex < static_cast<int32_t>(m_rows.size()) - 1)
        {
            ++m_currentIndex;
            LoadCurrentRow();
        }
        else
        {
            UpdateSelectionVisuals();
            UpdateMetrics();
        }
    }

    void MainWindow::SaveButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_rows.empty())
        {
            return;
        }

        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        UpdateTableRow(m_currentIndex);
        UpdateMetrics();
        StatusBarText().Text(L"Zm\u011Bny jsou zat\u00EDm ulo\u017Eeny pouze v pam\u011Bti");
    }

    void MainWindow::TargetTextBox_TextChanged(
        Windows::Foundation::IInspectable const&,
        TextChangedEventArgs const&)
    {
        if (m_loadingSelection || !m_initialized || m_rows.empty())
        {
            return;
        }

        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        row.status = L"Upraveno";

        TargetInfoText().Text(hstring{ L"#" + std::to_wstring(row.number) + L" \u00B7 upraven\u00FD p\u0159eklad" });
        TargetStatusText().Text(L"Stav: upraveno");
        UpdateTableRow(m_currentIndex);
        UpdateMetrics();
        StatusBarText().Text(L"Neulo\u017Een\u00E1 zm\u011Bna v aktu\u00E1ln\u00EDm titulku");
    }

    void MainWindow::SubtitleRow_Tapped(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&)
    {
        auto const index = RowIndexFromSender(sender);
        if (index < 0 || index >= static_cast<int32_t>(m_rows.size()) || index == m_currentIndex)
        {
            return;
        }

        m_currentIndex = index;
        LoadCurrentRow();
    }

    void MainWindow::LoadCurrentRow()
    {
        if (m_rows.empty())
        {
            return;
        }

        auto const& row = m_rows[m_currentIndex];
        m_loadingSelection = true;

        std::wstring header = L"Aktu\u00E1ln\u00ED titulek #" + std::to_wstring(row.number);
        header += L" \u00B7 ";
        header += row.start.c_str();
        header += L" \u2192 ";
        header += row.end.c_str();
        header += L" \u00B7 d\u00E9lka ";

        std::wostringstream durationStream;
        durationStream << std::fixed << std::setprecision(2) << row.duration;
        header += durationStream.str();
        header += L" s";
        HeaderCurrentSubtitleText().Text(hstring{ header });

        std::wstring timing = L"#" + std::to_wstring(row.number);
        timing += L" \u00B7 ";
        timing += row.start.c_str();
        timing += L" \u2192 ";
        timing += row.end.c_str();
        OriginalTimingText().Text(hstring{ timing });
        OriginalTextBox().Text(row.original);

        std::wstring targetInfo = L"#" + std::to_wstring(row.number) + L" \u00B7 ";
        targetInfo += row.status.c_str();
        TargetInfoText().Text(hstring{ targetInfo });
        TargetTextBox().Text(row.target);

        std::wstring status = L"Stav: ";
        status += row.status.c_str();
        TargetStatusText().Text(hstring{ status });

        TranscriptCurrentTimeText().Text(row.start);
        TranscriptCurrentText().Text(row.original);

        if (m_currentIndex > 0)
        {
            auto const& previous = m_rows[m_currentIndex - 1];
            TranscriptPreviousBlock().Visibility(Visibility::Visible);
            TranscriptPreviousTimeText().Text(previous.start);
            TranscriptPreviousText().Text(previous.original);
        }
        else
        {
            TranscriptPreviousBlock().Visibility(Visibility::Collapsed);
        }

        if (m_currentIndex < static_cast<int32_t>(m_rows.size()) - 1)
        {
            auto const& next = m_rows[m_currentIndex + 1];
            TranscriptNextBlock().Visibility(Visibility::Visible);
            TranscriptNextTimeText().Text(next.start);
            TranscriptNextText().Text(next.original);
        }
        else
        {
            TranscriptNextBlock().Visibility(Visibility::Collapsed);
        }

        std::wstring tablePosition = std::to_wstring(m_rows.size());
        tablePosition += L" \u0159\u00E1dk\u016F \u00B7 aktu\u00E1ln\u00ED #";
        tablePosition += std::to_wstring(row.number);
        TablePositionText().Text(hstring{ tablePosition });

        PreviousButton().IsEnabled(m_currentIndex > 0);
        NextButton().IsEnabled(m_currentIndex < static_cast<int32_t>(m_rows.size()) - 1);

        UpdateSelectionVisuals();
        UpdateMetrics();

        if (m_sourcePath.empty())
        {
            StatusBarText().Text(L"Uk\u00E1zkov\u00E1 data \u00B7 otev\u0159ete SRT/ASS/SSA tla\u010D\u00EDtkem Otev\u0159\u00EDt projekt");
        }
        else
        {
            auto const sourceName = std::filesystem::path(m_sourcePath.c_str()).filename().wstring();
            StatusBarText().Text(hstring{ L"Zdroj na\u010Dten p\u0159es Aegisub core \u00B7 " + sourceName });
        }

        m_loadingSelection = false;
    }

    void MainWindow::UpdateMetrics()
    {
        if (m_rows.empty())
        {
            TargetCplText().Text(L"CPL 0");
            TargetCpsText().Text(L"CPS 0.0");
            return;
        }

        auto const& row = m_rows[m_currentIndex];
        std::wstring const text{ row.target.c_str() };

        size_t currentLineLength = 0;
        size_t maxLineLength = 0;
        size_t characterCount = 0;

        for (auto const character : text)
        {
            if (character == L'\r')
            {
                continue;
            }

            if (character == L'\n')
            {
                maxLineLength = (std::max)(maxLineLength, currentLineLength);
                currentLineLength = 0;
                continue;
            }

            ++currentLineLength;
            ++characterCount;
        }

        maxLineLength = (std::max)(maxLineLength, currentLineLength);
        double const cps = row.duration > 0.0
            ? static_cast<double>(characterCount) / row.duration
            : 0.0;

        TargetCplText().Text(hstring{ L"CPL " + std::to_wstring(maxLineLength) });

        std::wostringstream cpsStream;
        cpsStream << L"CPS " << std::fixed << std::setprecision(1) << cps;
        TargetCpsText().Text(hstring{ cpsStream.str() });
    }

    void MainWindow::UpdateSelectionVisuals()
    {
        auto const accentBrush = TargetPanelBorder().BorderBrush();

        for (auto const& border : m_rowBorders)
        {
            border.BorderThickness(Thickness{ 0.0, 0.0, 0.0, 0.0 });
            border.BorderBrush(accentBrush);
        }

        if (m_currentIndex >= 0 && m_currentIndex < static_cast<int32_t>(m_rowBorders.size()))
        {
            m_rowBorders[m_currentIndex].BorderThickness(Thickness{ 4.0, 1.0, 0.0, 1.0 });
        }
    }

    void MainWindow::UpdateTableRow(int32_t index)
    {
        if (index < 0 || index >= static_cast<int32_t>(m_rows.size()) ||
            index >= static_cast<int32_t>(m_rowTargetTexts.size()) ||
            index >= static_cast<int32_t>(m_rowStatusTexts.size()))
        {
            return;
        }

        m_rowTargetTexts[index].Text(m_rows[index].target);
        m_rowStatusTexts[index].Text(m_rows[index].status);
    }

    int32_t MainWindow::RowIndexFromSender(
        Windows::Foundation::IInspectable const& sender) const
    {
        auto const element = sender.try_as<FrameworkElement>();
        if (!element)
        {
            return -1;
        }

        auto const name = element.Name();
        if (name == L"Row143Border") return 0;
        if (name == L"Row144Border") return 1;
        if (name == L"Row145Border") return 2;
        if (name == L"Row146Border") return 3;
        if (name == L"Row147Border") return 4;

        return -1;
    }

    void MainWindow::InitializeDynamicSubtitleGrid()
    {
        for (auto const& child : RootGrid().Children())
        {
            auto const outerBorder = child.try_as<Border>();
            if (!outerBorder || Grid::GetRow(outerBorder) != 2)
            {
                continue;
            }

            auto const container = outerBorder.Child().try_as<Grid>();
            if (!container)
            {
                continue;
            }

            for (auto const& containerChild : container.Children())
            {
                auto const scrollViewer = containerChild.try_as<ScrollViewer>();
                if (!scrollViewer || Grid::GetRow(scrollViewer) != 1)
                {
                    continue;
                }

                m_subtitleGrid = scrollViewer.Content().try_as<Grid>();
                if (m_subtitleGrid)
                {
                    return;
                }
            }
        }
    }

    void MainWindow::RebuildSubtitleGrid()
    {
        if (!m_subtitleGrid)
        {
            return;
        }

        auto const grid = m_subtitleGrid;
        grid.Children().Clear();
        grid.RowDefinitions().Clear();

        m_rowBorders.clear();
        m_rowTargetTexts.clear();
        m_rowStatusTexts.clear();

        RowDefinition headerRow;
        headerRow.Height(GridLength{ 34.0, GridUnitType::Pixel });
        grid.RowDefinitions().Append(headerRow);

        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            RowDefinition rowDefinition;
            rowDefinition.Height(GridLength{ 36.0, GridUnitType::Pixel });
            grid.RowDefinitions().Append(rowDefinition);
        }

        auto const accentBrush = TargetPanelBorder().BorderBrush();

        Border headerBorder;
        headerBorder.BorderBrush(accentBrush);
        headerBorder.BorderThickness(Thickness{ 0.0, 1.0, 0.0, 1.0 });
        Grid::SetRow(headerBorder, 0);
        Grid::SetColumnSpan(headerBorder, 6);
        grid.Children().Append(headerBorder);

        auto addText = [&](hstring const& text, int32_t row, int32_t column, bool ellipsis, double leftMargin)
        {
            TextBlock block;
            block.Text(text);
            block.Margin(Thickness{ leftMargin, 0.0, 4.0, 0.0 });
            block.VerticalAlignment(VerticalAlignment::Center);
            block.IsHitTestVisible(false);
            if (ellipsis)
            {
                block.TextTrimming(TextTrimming::CharacterEllipsis);
            }
            Grid::SetRow(block, row);
            Grid::SetColumn(block, column);
            grid.Children().Append(block);
            return block;
        };

        addText(L"#", 0, 0, false, 10.0);
        addText(L"Start", 0, 1, false, 8.0);
        addText(L"Konec", 0, 2, false, 8.0);
        addText(L"Origin\u00E1l", 0, 3, false, 8.0);
        addText(L"\u010Ce\u0161tina", 0, 4, false, 8.0);
        addText(L"Stav", 0, 5, false, 8.0);

        for (int32_t index = 0; index < static_cast<int32_t>(m_rows.size()); ++index)
        {
            auto const& row = m_rows[index];
            auto const visualRow = index + 1;

            Microsoft::UI::Xaml::Media::SolidColorBrush transparentBrush;
            transparentBrush.Color(Windows::UI::Color{ 0, 0, 0, 0 });

            Border rowBorder;
            rowBorder.Background(transparentBrush);
            rowBorder.BorderBrush(accentBrush);
            Grid::SetRow(rowBorder, visualRow);
            Grid::SetColumnSpan(rowBorder, 6);
            rowBorder.Tapped([this, index](auto const&, auto const&)
            {
                if (index < 0 || index >= static_cast<int32_t>(m_rows.size()) || index == m_currentIndex)
                {
                    return;
                }

                m_currentIndex = index;
                LoadCurrentRow();
            });
            grid.Children().Append(rowBorder);
            m_rowBorders.push_back(rowBorder);

            addText(hstring{ std::to_wstring(row.number) }, visualRow, 0, false, 10.0);
            addText(row.start, visualRow, 1, false, 8.0);
            addText(row.end, visualRow, 2, false, 8.0);
            addText(row.original, visualRow, 3, true, 8.0);
            auto const targetText = addText(row.target, visualRow, 4, true, 8.0);
            auto const statusText = addText(row.status, visualRow, 5, true, 8.0);
            m_rowTargetTexts.push_back(targetText);
            m_rowStatusTexts.push_back(statusText);
        }

        UpdateSelectionVisuals();
    }

    Microsoft::UI::Xaml::Controls::Button MainWindow::FindOpenProjectButton(
        Microsoft::UI::Xaml::DependencyObject const& root) const
    {
        if (!root)
        {
            return nullptr;
        }

        if (auto const button = root.try_as<Button>())
        {
            try
            {
                if (unbox_value<hstring>(button.Content()) == L"Otev\u0159\u00EDt projekt")
                {
                    return button;
                }
            }
            catch (...)
            {
            }
        }

        auto const childCount = Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < childCount; ++i)
        {
            auto const child = Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChild(root, i);
            if (auto const found = FindOpenProjectButton(child))
            {
                return found;
            }
        }

        return nullptr;
    }

    void MainWindow::HookOpenProjectButton()
    {
        auto const button = FindOpenProjectButton(RootGrid());
        if (!button)
        {
            StatusBarText().Text(L"Tla\u010D\u00EDtko Otev\u0159\u00EDt projekt nebylo nalezeno");
            return;
        }

        button.Click([this](auto const&, auto const&)
        {
            OpenSourceSubtitleFile();
        });
    }

    void MainWindow::OpenSourceSubtitleFile()
    {
        wchar_t filename[32768]{};
        wchar_t const filter[] =
            L"Titulky (*.srt;*.ass;*.ssa)\0*.srt;*.ass;*.ssa\0"
            L"V\u0161echny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = filename;
        dialog.nMaxFile = static_cast<DWORD>(std::size(filename));
        dialog.lpstrTitle = L"Otev\u0159\u00EDt zdrojov\u00E9 titulky";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&dialog))
        {
            return;
        }

        std::wstring errorMessage;
        if (!LoadSourceSubtitleFile(filename, errorMessage))
        {
            StatusBarText().Text(L"Na\u010Dten\u00ED titulk\u016F se nezda\u0159ilo");
            MessageBoxW(
                dialog.hwndOwner,
                errorMessage.c_str(),
                L"Aegisub Translation Workspace",
                MB_OK | MB_ICONERROR);
        }
    }

    bool MainWindow::LoadSourceSubtitleFile(std::wstring const& filename, std::wstring& errorMessage)
    {
        auto const bridge = FindBridgeExecutable();
        if (bridge.empty())
        {
            errorMessage =
                L"Nebyl nalezen aegisub-winui-bridge.exe.\n\n"
                L"Nejprve sestavte build\\src\\aegisub-winui-bridge.exe.";
            return false;
        }

        auto output = std::filesystem::temp_directory_path();
        output /= L"aegisub-winui-bridge-" + std::to_wstring(GetCurrentProcessId()) + L".tsv";

        std::error_code fileError;
        std::filesystem::remove(output, fileError);

        std::wstring commandLine = L"\"" + bridge.wstring() + L"\" \"" + filename + L"\" \"" + output.wstring() + L"\"";
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo{};

        if (!CreateProcessW(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo))
        {
            errorMessage = L"Nepoda\u0159ilo se spustit aegisub-winui-bridge.exe.";
            return false;
        }

        WaitForSingleObject(processInfo.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        if (!std::filesystem::exists(output))
        {
            errorMessage = L"Aegisub bridge nevytvo\u0159il v\u00FDstupn\u00ED soubor.";
            return false;
        }

        std::ifstream stream(output, std::ios::binary);
        if (!stream)
        {
            errorMessage = L"V\u00FDstup Aegisub bridge nelze otev\u0159\u00EDt.";
            std::filesystem::remove(output, fileError);
            return false;
        }

        std::string line;
        if (!std::getline(stream, line))
        {
            errorMessage = L"Aegisub bridge vr\u00E1til pr\u00E1zdn\u00FD v\u00FDstup.";
            std::filesystem::remove(output, fileError);
            return false;
        }

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.rfind("ERROR\t", 0) == 0)
        {
            errorMessage = ToWide(UnescapeBridgeField(line.substr(6)));
            std::filesystem::remove(output, fileError);
            return false;
        }

        if (line != "AEGISUB-WINUI-BRIDGE\t1")
        {
            errorMessage = L"Aegisub bridge vr\u00E1til nezn\u00E1m\u00FD form\u00E1t dat.";
            std::filesystem::remove(output, fileError);
            return false;
        }

        if (exitCode != 0)
        {
            errorMessage = L"Aegisub bridge skon\u010Dil s chybou " + std::to_wstring(exitCode) + L".";
            std::filesystem::remove(output, fileError);
            return false;
        }

        std::vector<SubtitleRowData> loadedRows;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            auto const firstTab = line.find('\t');
            auto const secondTab = firstTab == std::string::npos
                ? std::string::npos
                : line.find('\t', firstTab + 1);

            if (firstTab == std::string::npos || secondTab == std::string::npos)
            {
                continue;
            }

            auto const start = line.substr(0, firstTab);
            auto const end = line.substr(firstTab + 1, secondTab - firstTab - 1);
            auto const original = UnescapeBridgeField(line.substr(secondTab + 1));

            SubtitleRowData row;
            row.number = static_cast<int32_t>(loadedRows.size()) + 1;
            row.start = to_hstring(start);
            row.end = to_hstring(end);
            row.duration = (std::max)(0.0, TimestampSeconds(end) - TimestampSeconds(start));
            row.original = to_hstring(original);
            row.target = L"";
            row.status = L"P\u0159ipraveno";
            loadedRows.push_back(std::move(row));
        }

        stream.close();
        std::filesystem::remove(output, fileError);

        if (loadedRows.empty())
        {
            errorMessage = L"Soubor neobsahuje \u017E\u00E1dn\u00E9 dialogov\u00E9 titulky.";
            return false;
        }

        m_rows = std::move(loadedRows);
        m_currentIndex = 0;
        m_sourcePath = hstring{ filename };
        RebuildSubtitleGrid();
        LoadCurrentRow();
        return true;
    }
}
