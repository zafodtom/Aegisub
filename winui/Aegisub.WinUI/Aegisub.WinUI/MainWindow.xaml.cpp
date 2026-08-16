#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

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
        LoadCurrentRow();
    }

    void MainWindow::PreviousButton_Click(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_currentIndex <= 0)
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
        if (m_currentIndex >= static_cast<int32_t>(m_rows.size()) - 1)
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
        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        row.status = L"Schváleno";

        UpdateTableRow(m_currentIndex);
        TargetInfoText().Text(hstring{ L"#" + std::to_wstring(row.number) + L" · schváleno" });
        TargetStatusText().Text(L"Stav: schváleno");
        StatusBarText().Text(L"Testovací režim · titulek schválen");

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
        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        UpdateTableRow(m_currentIndex);
        UpdateMetrics();
        StatusBarText().Text(L"Testovací režim · změny jsou uloženy pouze v paměti");
    }

    void MainWindow::TargetTextBox_TextChanged(
        Windows::Foundation::IInspectable const&,
        TextChangedEventArgs const&)
    {
        if (m_loadingSelection || !m_initialized)
        {
            return;
        }

        auto& row = m_rows[m_currentIndex];
        row.target = TargetTextBox().Text();
        row.status = L"Upraveno";

        TargetInfoText().Text(hstring{ L"#" + std::to_wstring(row.number) + L" · upravený překlad" });
        TargetStatusText().Text(L"Stav: upraveno");
        UpdateTableRow(m_currentIndex);
        UpdateMetrics();
        StatusBarText().Text(L"Testovací režim · neuložená změna v aktuálním titulku");
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
        auto const& row = m_rows[m_currentIndex];
        m_loadingSelection = true;

        std::wstring header = L"Aktuální titulek #" + std::to_wstring(row.number);
        header += L" · ";
        header += row.start.c_str();
        header += L" → ";
        header += row.end.c_str();
        header += L" · délka ";

        std::wostringstream durationStream;
        durationStream << std::fixed << std::setprecision(2) << row.duration;
        header += durationStream.str();
        header += L" s";
        HeaderCurrentSubtitleText().Text(hstring{ header });

        std::wstring timing = L"#" + std::to_wstring(row.number);
        timing += L" · ";
        timing += row.start.c_str();
        timing += L" → ";
        timing += row.end.c_str();
        OriginalTimingText().Text(hstring{ timing });
        OriginalTextBox().Text(row.original);

        std::wstring targetInfo = L"#" + std::to_wstring(row.number) + L" · ";
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

        std::wstring tablePosition = L"5 testovacích řádků · aktuální #" + std::to_wstring(row.number);
        TablePositionText().Text(hstring{ tablePosition });

        PreviousButton().IsEnabled(m_currentIndex > 0);
        NextButton().IsEnabled(m_currentIndex < static_cast<int32_t>(m_rows.size()) - 1);

        UpdateSelectionVisuals();
        UpdateMetrics();

        StatusBarText().Text(L"Testovací data · bez napojení na Aegisub core");
        m_loadingSelection = false;
    }

    void MainWindow::UpdateMetrics()
    {
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
        std::array<Border, 5> const borders
        {
            Row143Border(),
            Row144Border(),
            Row145Border(),
            Row146Border(),
            Row147Border(),
        };

        auto const accentBrush = TargetPanelBorder().BorderBrush();

        for (auto const& border : borders)
        {
            border.BorderThickness(Thickness{ 0.0, 0.0, 0.0, 0.0 });
            border.BorderBrush(accentBrush);
        }

        borders[m_currentIndex].BorderThickness(Thickness{ 4.0, 1.0, 0.0, 1.0 });
    }

    void MainWindow::UpdateTableRow(int32_t index)
    {
        auto const& row = m_rows[index];

        switch (index)
        {
        case 0:
            Row143TargetText().Text(row.target);
            Row143StatusText().Text(row.status);
            break;
        case 1:
            Row144TargetText().Text(row.target);
            Row144StatusText().Text(row.status);
            break;
        case 2:
            Row145TargetText().Text(row.target);
            Row145StatusText().Text(row.status);
            break;
        case 3:
            Row146TargetText().Text(row.target);
            Row146StatusText().Text(row.status);
            break;
        case 4:
            Row147TargetText().Text(row.target);
            Row147StatusText().Text(row.status);
            break;
        default:
            break;
        }
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
}
