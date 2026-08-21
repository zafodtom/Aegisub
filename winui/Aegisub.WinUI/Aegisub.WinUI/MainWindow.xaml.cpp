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
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "Comdlg32.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace
{
    struct SingleCharacterEdit
    {
        int kind{};
        size_t position{};
    };

    SingleCharacterEdit ClassifySingleCharacterEdit(std::wstring_view before, std::wstring_view after)
    {
        int kind = 0;
        if (after.size() == before.size() + 1)
        {
            kind = 1;
        }
        else if (before.size() == after.size() + 1)
        {
            kind = 2;
        }
        else
        {
            return {};
        }

        auto const& shorter = kind == 1 ? before : after;
        auto const& longer = kind == 1 ? after : before;
        size_t position = 0;
        while (position < shorter.size() && shorter[position] == longer[position])
        {
            ++position;
        }
        if (shorter.substr(position) != longer.substr(position + 1))
        {
            return {};
        }
        return { kind, position };
    }

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

    std::string EscapeBridgeField(std::string const& value)
    {
        std::string output;
        output.reserve(value.size());

        for (char c : value)
        {
            switch (c)
            {
            case '\\': output += "\\\\"; break;
            case '\t': output += "\\t"; break;
            case '\r': output += "\\r"; break;
            case '\n': output += "\\n"; break;
            default: output.push_back(c); break;
            }
        }

        return output;
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

    bool RunProcess(std::wstring commandLine, DWORD& exitCode)
    {
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
            return false;
        }

        WaitForSingleObject(processInfo.hProcess, INFINITE);
        exitCode = 0;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return true;
    }

    bool ReadBridgeError(std::filesystem::path const& output, std::wstring& errorMessage)
    {
        std::ifstream stream(output, std::ios::binary);
        if (!stream)
        {
            return false;
        }

        std::string line;
        if (!std::getline(stream, line))
        {
            return false;
        }
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.rfind("ERROR\t", 0) != 0)
        {
            return false;
        }

        errorMessage = ToWide(UnescapeBridgeField(line.substr(6)));
        return true;
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
        HookWindowClosing();
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

        MoveCurrentBy(-1);
    }

    void MainWindow::NextButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_rows.empty() || m_currentIndex >= static_cast<int32_t>(m_rows.size()) - 1)
        {
            return;
        }

        MoveCurrentBy(1);
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
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size()))
        {
            m_targetEntries[m_currentIndex].text = row.target;
        }

        UpdateTableRow(m_currentIndex);
        TargetInfoText().Text(hstring{ L"#" + std::to_wstring(row.number) + L" \u00B7 schv\u00E1leno" });
        TargetStatusText().Text(L"Stav: schv\u00E1leno");
        StatusBarText().Text(L"Titulek schv\u00E1len");

        if (m_currentIndex < static_cast<int32_t>(m_rows.size()) - 1)
        {
            StoreCurrentEditorSelection();
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
        SaveFromShortcut();
    }

    void MainWindow::SaveAsButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        SaveAsFromShortcut();
    }

    void MainWindow::OpenBothButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        OpenProjectFiles();
    }

    void MainWindow::OpenSourceButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        OpenSourceFile();
    }

    void MainWindow::OpenTargetButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        OpenTargetFile();
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
        auto const newText = TargetTextBox().Text();
        bool const applyingHistory = m_hasPendingHistoryText && newText == m_pendingHistoryText;
        if (applyingHistory)
        {
            m_hasPendingHistoryText = false;
            m_pendingHistoryText = L"";
        }
        if (!row.historyInitialized)
        {
            row.savedTarget = row.target;
            row.historyInitialized = true;
        }
        if (!applyingHistory && newText != row.target)
        {
            std::wstring const before{ row.target.c_str() };
            std::wstring const after{ newText.c_str() };
            auto const edit = ClassifySingleCharacterEdit(before, after);
            bool const continuesInsertion = edit.kind == 1
                && row.editSequenceKind == 1
                && edit.position == row.editSequencePosition;
            bool const continuesDeletion = edit.kind == 2
                && row.editSequenceKind == 2
                && (edit.position == row.editSequencePosition
                    || edit.position + 1 == row.editSequencePosition);

            if (!continuesInsertion && !continuesDeletion)
            {
                row.undoHistory.push_back(row.target);
                if (row.undoHistory.size() > 200)
                {
                    row.undoHistory.erase(row.undoHistory.begin());
                }
            }
            row.redoHistory.clear();
            row.editSequenceKind = edit.kind;
            row.editSequencePosition = edit.position + (edit.kind == 1 ? 1 : 0);
        }
        else if (applyingHistory)
        {
            row.editSequenceKind = 0;
        }

        row.target = newText;
        row.targetModified = row.target != row.savedTarget;
        row.status = row.targetModified ? L"Upraveno" : L"Ulo\u017Eeno";
        UpdateDirtyFromRows();
        if (m_currentIndex < static_cast<int32_t>(m_targetEntries.size()))
        {
            m_targetEntries[m_currentIndex].text = row.target;
        }

        TargetInfoText().Text(hstring{ L"#" + std::to_wstring(row.number) +
            (row.targetModified ? L" \u00B7 upraven\u00FD p\u0159eklad" : L" \u00B7 ulo\u017Een\u00E1 verze") });
        TargetStatusText().Text(row.targetModified ? L"Stav: upraveno" : L"Stav: ulo\u017Eeno");
        UpdateTableRow(m_currentIndex);
        UpdateMetrics();
        StatusBarText().Text(row.targetModified
            ? L"Neulo\u017Een\u00E1 zm\u011Bna v aktu\u00E1ln\u00EDm titulku"
            : L"Text odpov\u00EDd\u00E1 ulo\u017Een\u00E9 verzi");
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

        StoreCurrentEditorSelection();
        m_currentIndex = index;
        LoadCurrentRow();
    }

    void MainWindow::LoadCurrentRow()
    {
        if (m_rows.empty())
        {
            return;
        }

        auto& row = m_rows[m_currentIndex];
        row.editSequenceKind = 0;
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

        if (row.sourceStart.empty())
        {
            OriginalTimingText().Text(hstring{ L"#" + std::to_wstring(row.number) + L" \u00B7 bez \u010Dasov\u00E9ho p\u00E1ru" });
        }
        else
        {
            std::wstring timing = L"#" + std::to_wstring(row.number);
            timing += L" \u00B7 ";
            timing += row.sourceStart.c_str();
            timing += L" \u2192 ";
            timing += row.sourceEnd.c_str();
            OriginalTimingText().Text(hstring{ timing });
        }
        OriginalTextBox().Text(row.original);

        std::wstring targetInfo = L"#" + std::to_wstring(row.number) + L" \u00B7 ";
        targetInfo += row.status.c_str();
        TargetInfoText().Text(hstring{ targetInfo });
        TargetTextBox().Text(row.target);
        if (row.selectionInitialized)
        {
            auto const textLength = static_cast<int32_t>(row.target.size());
            auto const selectionStart = (std::max)(0, (std::min)(textLength, row.selectionStart));
            auto const selectionLength = (std::max)(0,
                (std::min)(textLength - selectionStart, row.selectionLength));
            TargetTextBox().SelectionStart(selectionStart);
            TargetTextBox().SelectionLength(selectionLength);
        }
        else
        {
            TargetTextBox().SelectionStart(0);
            TargetTextBox().SelectionLength(0);
        }

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

        RefreshProgressSummary();

        PreviousButton().IsEnabled(m_currentIndex > 0);
        NextButton().IsEnabled(m_currentIndex < static_cast<int32_t>(m_rows.size()) - 1);

        UpdateSelectionVisuals();
        ScrollCurrentRowIntoView();
        UpdateMetrics();

        if (m_sourcePath.empty())
        {
            StatusBarText().Text(L"Uk\u00E1zkov\u00E1 data \u00B7 tla\u010D\u00EDtko Otev\u0159\u00EDt projekt na\u010Dte origin\u00E1l a \u010De\u0161tinu");
        }
        else if (m_targetPath.empty())
        {
            auto const sourceName = std::filesystem::path(m_sourcePath.c_str()).filename().wstring();
            StatusBarText().Text(hstring{ L"Origin\u00E1l: " + sourceName + L" \u00B7 \u010De\u0161tina nen\u00ED na\u010Dtena" });
        }
        else
        {
            auto const sourceName = std::filesystem::path(m_sourcePath.c_str()).filename().wstring();
            auto const targetName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
            StatusBarText().Text(hstring{
                L"Origin\u00E1l: " + sourceName + L" \u00B7 \u010Ce\u0161tina: " + targetName + L" \u00B7 p\u00E1rov\u00E1n\u00ED podle \u010Dasu" });
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

    void MainWindow::StoreCurrentEditorSelection()
    {
        if (m_loadingSelection || m_rows.empty() || m_currentIndex < 0
            || m_currentIndex >= static_cast<int32_t>(m_rows.size()))
        {
            return;
        }

        auto& row = m_rows[m_currentIndex];
        row.selectionStart = TargetTextBox().SelectionStart();
        row.selectionLength = TargetTextBox().SelectionLength();
        row.selectionInitialized = true;
        row.editSequenceKind = 0;
    }

    void MainWindow::ScrollCurrentRowIntoView()
    {
        if (m_currentIndex < 0 || m_currentIndex >= static_cast<int32_t>(m_rowBorders.size()))
        {
            return;
        }

        m_rowBorders[m_currentIndex].StartBringIntoView();
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

                StoreCurrentEditorSelection();
                m_currentIndex = index;
                LoadCurrentRow();
                TargetTextBox().Focus(FocusState::Programmatic);
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

    void MainWindow::SetDirty(bool dirty)
    {
        m_hasUnsavedChanges = dirty;
        Title(dirty
            ? L"Aegisub Translation Workspace *"
            : L"Aegisub Translation Workspace");
    }

    bool MainWindow::ConfirmSaveBefore(std::wstring const& action)
    {
        if (!m_hasUnsavedChanges)
        {
            return true;
        }

        auto const message =
            L"Projekt obsahuje neulo\u017Een\u00E9 zm\u011Bny.\n\nChcete je ulo\u017Eit p\u0159ed " + action + L"?";
        auto const result = MessageBoxW(
            GetActiveWindow(),
            message.c_str(),
            L"Aegisub Translation Workspace",
            MB_YESNOCANCEL | MB_ICONWARNING);

        if (result == IDCANCEL)
        {
            return false;
        }
        if (result == IDNO)
        {
            return true;
        }

        std::wstring destination;
        if (m_targetPath.empty() && !SelectSubtitleSaveFile(destination))
            return false;

        std::wstring errorMessage;
        if (SaveTargetSubtitleFile(errorMessage, destination))
        {
            return true;
        }

        MessageBoxW(
            GetActiveWindow(),
            errorMessage.c_str(),
            L"Ulo\u017Een\u00ED se nezda\u0159ilo",
            MB_OK | MB_ICONERROR);
        return false;
    }

    void MainWindow::HookWindowClosing()
    {
        if (m_windowClosingHookInstalled)
        {
            return;
        }

        m_windowClosingHookInstalled = true;
        AppWindow().Closing([this](
            Microsoft::UI::Windowing::AppWindow const&,
            Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
        {
            if (!ConfirmSaveBefore(L"zav\u0159en\u00EDm aplikace"))
            {
                args.Cancel(true);
            }
        });
    }

    bool MainWindow::SelectSubtitleFile(std::wstring const& title, std::wstring& filename) const
    {
        wchar_t buffer[32768]{};
        wchar_t const filter[] =
            L"Titulky (*.srt;*.ass;*.ssa)\0*.srt;*.ass;*.ssa\0"
            L"V\u0161echny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = title.c_str();
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&dialog))
        {
            return false;
        }

        filename = buffer;
        return true;
    }

    bool MainWindow::SelectSubtitleSaveFile(std::wstring& filename) const
    {
        wchar_t buffer[32768]{};
        std::wstring suggestedName;
        if (!m_targetPath.empty())
        {
            suggestedName = std::filesystem::path(m_targetPath.c_str()).filename().wstring();
        }
        else if (!m_sourcePath.empty())
        {
            auto source = std::filesystem::path(m_sourcePath.c_str());
            suggestedName = source.stem().wstring() + L".cs" + source.extension().wstring();
        }
        if (!suggestedName.empty())
        {
            wcsncpy_s(buffer, suggestedName.c_str(), _TRUNCATE);
        }

        wchar_t const filter[] =
            L"Titulky SubRip (*.srt)\0*.srt\0"
            L"Titulky Advanced SubStation Alpha (*.ass)\0*.ass\0"
            L"Titulky SubStation Alpha (*.ssa)\0*.ssa\0"
            L"V\u0161echny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Ulo\u017Eit \u010Desk\u00E9 titulky jako";
        dialog.lpstrDefExt = L"srt";
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

        if (!GetSaveFileNameW(&dialog))
            return false;

        filename = buffer;
        return true;
    }

    void MainWindow::OpenProjectFiles()
    {
        if (!ConfirmSaveBefore(L"otev\u0159en\u00EDm jin\u00E9ho projektu"))
        {
            return;
        }

        std::wstring sourceFilename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt origin\u00E1ln\u00ED titulky", sourceFilename))
        {
            return;
        }

        std::vector<SubtitleEntry> sourceEntries;
        std::wstring errorMessage;
        if (!ReadSubtitleFile(sourceFilename, sourceEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        std::wstring targetFilename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt p\u0159ipraven\u00E9 \u010Desk\u00E9 titulky", targetFilename))
        {
            return;
        }

        std::vector<SubtitleEntry> targetEntries;
        if (!ReadSubtitleFile(targetFilename, targetEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        m_sourceEntries = std::move(sourceEntries);
        m_sourcePath = hstring{ sourceFilename };
        m_targetEntries = std::move(targetEntries);
        m_targetPath = hstring{ targetFilename };
        RefreshLoadedProject();
    }

    void MainWindow::OpenSourceFile()
    {
        if (!ConfirmSaveBefore(L"otev\u0159en\u00EDm jin\u00E9ho origin\u00E1lu"))
            return;

        std::wstring filename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt origin\u00E1ln\u00ED titulky", filename))
            return;

        std::vector<SubtitleEntry> entries;
        std::wstring errorMessage;
        if (!ReadSubtitleFile(filename, entries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        std::vector<SubtitleEntry> refreshedTargetEntries;
        if (!m_targetPath.empty()
            && !ReadSubtitleFile(m_targetPath.c_str(), refreshedTargetEntries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        m_sourceEntries = std::move(entries);
        m_sourcePath = hstring{ filename };
        if (!m_targetPath.empty())
            m_targetEntries = std::move(refreshedTargetEntries);
        RefreshLoadedProject();
    }

    void MainWindow::OpenTargetFile()
    {
        if (!ConfirmSaveBefore(L"otev\u0159en\u00EDm jin\u00E9ho \u010Desk\u00E9ho p\u0159ekladu"))
            return;

        std::wstring filename;
        if (!SelectSubtitleFile(L"Otev\u0159\u00EDt p\u0159ipraven\u00E9 \u010Desk\u00E9 titulky", filename))
            return;

        std::vector<SubtitleEntry> entries;
        std::wstring errorMessage;
        if (!ReadSubtitleFile(filename, entries, errorMessage))
        {
            MessageBoxW(GetActiveWindow(), errorMessage.c_str(), L"Aegisub Translation Workspace", MB_OK | MB_ICONERROR);
            return;
        }

        m_targetEntries = std::move(entries);
        m_targetPath = hstring{ filename };
        RefreshLoadedProject();
    }

    void MainWindow::RefreshLoadedProject()
    {
        BuildAlignedRows();
        InitializeWorkflowStatuses();
        RefreshQaAll();
        RebuildSubtitleGrid();
        LoadCurrentRow();
        RefreshCurrentQaVisuals();
        RefreshProjectFileLabels();
        SetDirty(false);
    }

    void MainWindow::RefreshProjectFileLabels()
    {
        auto update = [](TextBlock const& label, hstring const& path, wchar_t const* emptyText)
        {
            if (path.empty())
            {
                label.Text(emptyText);
                ToolTipService::SetToolTip(label, nullptr);
                return;
            }

            auto const filename = std::filesystem::path(path.c_str()).filename().wstring();
            label.Text(hstring{ filename });
            ToolTipService::SetToolTip(label, box_value(path));
        };

        update(OriginalFileText(), m_sourcePath, L"Soubor nen\u00ED na\u010Dten");
        update(TargetFileText(), m_targetPath, L"Nov\u00FD p\u0159eklad \u00B7 zat\u00EDm neulo\u017Een");
    }

    bool MainWindow::IsTranslationEmpty(hstring const& text) const
    {
        std::wstring const value{ text.c_str() };
        return value.empty() || std::all_of(value.begin(), value.end(), [](wchar_t character)
        {
            return std::iswspace(character) != 0;
        });
    }

    void MainWindow::RefreshProgressSummary()
    {
        auto const untranslatedCount = std::count_if(m_rows.begin(), m_rows.end(), [this](auto const& row)
        {
            return IsTranslationEmpty(row.target);
        });
        auto const issueCount = std::count_if(m_rows.begin(), m_rows.end(), [](auto const& row)
        {
            return !row.qaIssue.empty();
        });
        auto const translatedCount = m_rows.size() - static_cast<size_t>(untranslatedCount);

        NextUntranslatedButton().Content(box_value(hstring{
            L"Dal\u0161\u00ED nep\u0159elo\u017Een\u00FD (" + std::to_wstring(untranslatedCount) + L")" }));
        NextUntranslatedButton().IsEnabled(untranslatedCount > 0);

        std::wstring summary = std::to_wstring(translatedCount) + L"/" +
            std::to_wstring(m_rows.size()) + L" p\u0159elo\u017Eeno \u00B7 probl\u00E9my " +
            std::to_wstring(issueCount);
        if (!m_rows.empty() && m_currentIndex >= 0 && m_currentIndex < static_cast<int32_t>(m_rows.size()))
            summary += L" \u00B7 aktu\u00E1ln\u00ED #" + std::to_wstring(m_rows[m_currentIndex].number);
        TablePositionText().Text(hstring{ summary });
    }

    bool MainWindow::ReadSubtitleFile(
        std::wstring const& filename,
        std::vector<SubtitleEntry>& entries,
        std::wstring& errorMessage) const
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
        output /= L"aegisub-winui-read-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L".tsv";

        std::error_code fileError;
        std::filesystem::remove(output, fileError);

        std::wstring commandLine = L"\"" + bridge.wstring() + L"\" \"" + filename + L"\" \"" + output.wstring() + L"\"";
        DWORD exitCode = 0;
        if (!RunProcess(commandLine, exitCode))
        {
            errorMessage = L"Nepoda\u0159ilo se spustit aegisub-winui-bridge.exe.";
            return false;
        }

        if (!std::filesystem::exists(output))
        {
            errorMessage = L"Aegisub bridge nevytvo\u0159il v\u00FDstupn\u00ED soubor. K\u00F3d: " + std::to_wstring(exitCode) + L".";
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

        bool const protocolV1 = line == "AEGISUB-WINUI-BRIDGE\t1";
        bool const protocolV2 = line == "AEGISUB-WINUI-BRIDGE\t2";
        if (!protocolV1 && !protocolV2)
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

        entries.clear();
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

            std::string displayText;
            std::string rawText;
            if (protocolV2)
            {
                auto const thirdTab = line.find('\t', secondTab + 1);
                if (thirdTab == std::string::npos)
                {
                    continue;
                }
                displayText = UnescapeBridgeField(line.substr(secondTab + 1, thirdTab - secondTab - 1));
                rawText = UnescapeBridgeField(line.substr(thirdTab + 1));
            }
            else
            {
                displayText = UnescapeBridgeField(line.substr(secondTab + 1));
                rawText = displayText;
            }

            SubtitleEntry entry;
            entry.start = to_hstring(start);
            entry.end = to_hstring(end);
            entry.startSeconds = TimestampSeconds(start);
            entry.endSeconds = TimestampSeconds(end);
            entry.duration = (std::max)(0.0, entry.endSeconds - entry.startSeconds);
            entry.text = to_hstring(displayText);
            entry.rawText = to_hstring(rawText);
            entries.push_back(std::move(entry));
        }

        stream.close();
        std::filesystem::remove(output, fileError);

        if (entries.empty())
        {
            errorMessage = L"Soubor neobsahuje \u017E\u00E1dn\u00E9 dialogov\u00E9 titulky.";
            return false;
        }

        return true;
    }

    void MainWindow::BuildAlignedRows()
    {
        m_rows.clear();

        if (!m_targetEntries.empty())
        {
            m_rows.reserve(m_targetEntries.size());

            for (size_t targetIndex = 0; targetIndex < m_targetEntries.size(); ++targetIndex)
            {
                auto const& target = m_targetEntries[targetIndex];
                SubtitleRowData row;
                row.number = static_cast<int32_t>(targetIndex) + 1;
                row.start = target.start;
                row.end = target.end;
                row.duration = target.duration;
                row.target = target.text;
                row.rawTarget = target.rawText;
                row.savedTarget = row.target;
                row.status = L"P\u0159ipraveno";
                row.targetModified = false;
                row.historyInitialized = true;

                bool matched = false;
                std::wstring original;

                for (auto const& source : m_sourceEntries)
                {
                    double const overlap = (std::min)(target.endSeconds, source.endSeconds) -
                        (std::max)(target.startSeconds, source.startSeconds);
                    if (overlap <= 0.0)
                    {
                        continue;
                    }

                    if (!original.empty())
                    {
                        original += L"\n";
                    }
                    original += source.text.c_str();
                    if (!matched)
                    {
                        row.sourceStart = source.start;
                    }
                    row.sourceEnd = source.end;
                    matched = true;
                }

                row.original = hstring{ original };
                if (!matched)
                {
                    row.sourceStart = L"";
                    row.sourceEnd = L"";
                }

                m_rows.push_back(std::move(row));
            }
        }
        else
        {
            m_rows.reserve(m_sourceEntries.size());
            for (size_t sourceIndex = 0; sourceIndex < m_sourceEntries.size(); ++sourceIndex)
            {
                auto const& source = m_sourceEntries[sourceIndex];
                SubtitleRowData row;
                row.number = static_cast<int32_t>(sourceIndex) + 1;
                row.start = source.start;
                row.end = source.end;
                row.duration = source.duration;
                row.sourceStart = source.start;
                row.sourceEnd = source.end;
                row.original = source.text;
                row.target = L"";
                row.rawTarget = L"";
                row.savedTarget = row.target;
                row.status = L"P\u0159ipraveno";
                row.targetModified = false;
                row.historyInitialized = true;
                m_rows.push_back(std::move(row));
            }
        }

        m_currentIndex = 0;
    }

    bool MainWindow::SaveTargetSubtitleFile(std::wstring& errorMessage, std::wstring const& destinationPath)
    {
        auto const savePath = destinationPath.empty() ? std::wstring{ m_targetPath.c_str() } : destinationPath;
        auto const templatePath = m_targetPath.empty()
            ? std::wstring{ m_sourcePath.c_str() }
            : std::wstring{ m_targetPath.c_str() };
        if (savePath.empty() || templatePath.empty() || m_rows.empty())
        {
            errorMessage = L"Nejd\u0159\u00EDve na\u010Dt\u011Bte origin\u00E1ln\u00ED nebo p\u0159ipraven\u00E9 \u010Desk\u00E9 titulky.";
            return false;
        }

        if (!m_sourcePath.empty())
        {
            std::error_code pathError;
            auto const sourceAbsolute = std::filesystem::absolute(
                std::filesystem::path(m_sourcePath.c_str()), pathError).lexically_normal().wstring();
            pathError.clear();
            auto const saveAbsolute = std::filesystem::absolute(
                std::filesystem::path(savePath), pathError).lexically_normal().wstring();
            if (!pathError && _wcsicmp(sourceAbsolute.c_str(), saveAbsolute.c_str()) == 0)
            {
                errorMessage = L"Český překlad nelze uložit přes soubor originálu. Zvolte jiný název souboru.";
                return false;
            }
        }

        if (!m_targetEntries.empty() && m_rows.size() != m_targetEntries.size())
        {
            errorMessage = L"Po\u010Det pracovn\u00EDch \u0159\u00E1dk\u016F neodpov\u00EDd\u00E1 \u010Desk\u00E9mu souboru.";
            return false;
        }

        auto const bridge = FindBridgeExecutable();
        if (bridge.empty())
        {
            errorMessage = L"Nebyl nalezen aegisub-winui-bridge.exe.";
            return false;
        }

        auto const targetPath = std::filesystem::path(savePath);
        auto updateFile = std::filesystem::temp_directory_path();
        updateFile /= L"aegisub-winui-write-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L".tsv";

        auto tempOutput = targetPath.parent_path();
        tempOutput /= targetPath.filename().wstring() +
            L".winui-" + std::to_wstring(GetCurrentProcessId()) +
            L"-" + std::to_wstring(GetTickCount64()) + L".tmp";

        std::error_code fileError;
        std::filesystem::remove(updateFile, fileError);
        std::filesystem::remove(tempOutput, fileError);

        {
            std::ofstream stream(updateFile, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                errorMessage = L"Nelze vytvo\u0159it do\u010Dasn\u00FD soubor pro ulo\u017Een\u00ED titulk\u016F.";
                return false;
            }

            stream << "AEGISUB-WINUI-BRIDGE\t1\n";
            for (auto const& row : m_rows)
            {
                auto const textForSave = row.targetModified ? row.target : row.rawTarget;
                stream << to_string(row.start) << '\t'
                       << to_string(row.end) << '\t'
                       << EscapeBridgeField(to_string(textForSave)) << '\n';
            }
        }

        std::wstring commandLine = L"\"" + bridge.wstring() + L"\" --write \"" +
            templatePath + L"\" \"" + updateFile.wstring() + L"\" \"" + tempOutput.wstring() + L"\"";

        DWORD exitCode = 0;
        if (!RunProcess(commandLine, exitCode))
        {
            std::filesystem::remove(updateFile, fileError);
            errorMessage = L"Nepoda\u0159ilo se spustit Aegisub bridge pro ulo\u017Een\u00ED.";
            return false;
        }

        std::filesystem::remove(updateFile, fileError);

        if (exitCode != 0)
        {
            if (!ReadBridgeError(tempOutput, errorMessage))
            {
                errorMessage = L"Aegisub bridge skon\u010Dil p\u0159i ukl\u00E1d\u00E1n\u00ED s chybou " + std::to_wstring(exitCode) + L".";
            }
            std::filesystem::remove(tempOutput, fileError);
            return false;
        }

        if (!std::filesystem::exists(tempOutput))
        {
            errorMessage = L"Aegisub bridge nevytvo\u0159il ulo\u017Een\u00FD soubor.";
            return false;
        }

        if (!MoveFileExW(
            tempOutput.c_str(),
            targetPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            errorMessage = L"Do\u010Dasn\u00FD soubor se poda\u0159ilo vytvo\u0159it, ale nepoda\u0159ilo se nahradit p\u016Fvodn\u00ED \u010Desk\u00FD soubor.";
            std::filesystem::remove(tempOutput, fileError);
            return false;
        }

        if (m_targetEntries.empty())
        {
            m_targetEntries = m_sourceEntries;
        }
        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            auto savedRaw = m_rows[i].targetModified ? m_rows[i].target : m_rows[i].rawTarget;
            m_rows[i].rawTarget = savedRaw;
            m_rows[i].savedTarget = m_rows[i].target;
            m_rows[i].historyInitialized = true;
            m_rows[i].targetModified = false;
            m_rows[i].editSequenceKind = 0;
            m_targetEntries[i].start = m_rows[i].start;
            m_targetEntries[i].end = m_rows[i].end;
            m_targetEntries[i].text = m_rows[i].target;
            m_targetEntries[i].rawText = savedRaw;
        }

        m_targetPath = hstring{ savePath };
        RefreshProjectFileLabels();
        SetDirty(false);

        return true;
    }
}
