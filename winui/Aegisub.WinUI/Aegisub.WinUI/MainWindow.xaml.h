#pragma once

#include "MainWindow.g.h"

#include <vector>

namespace winrt::Aegisub_WinUI::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow()
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void RootGrid_Loaded(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void PreviousButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void NextButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void ApproveButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void SaveButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void TargetTextBox_TextChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);

        void SubtitleRow_Tapped(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& args);

    private:
        struct SubtitleEntry
        {
            winrt::hstring start;
            winrt::hstring end;
            double startSeconds{};
            double endSeconds{};
            double duration{};
            winrt::hstring text;
        };

        struct SubtitleRowData
        {
            int32_t number{};
            winrt::hstring start;
            winrt::hstring end;
            double duration{};
            winrt::hstring sourceStart;
            winrt::hstring sourceEnd;
            winrt::hstring original;
            winrt::hstring target;
            winrt::hstring status;
        };

        std::vector<SubtitleRowData> m_rows
        {
            { 143, L"00:12:38.310", L"00:12:41.420", 3.11, L"00:12:38.310", L"00:12:41.420", L"We still have time.", L"Po\u0159\u00E1d m\u00E1me \u010Das.", L"Schv\u00E1leno" },
            { 144, L"00:12:41.980", L"00:12:43.980", 2.00, L"00:12:41.980", L"00:12:43.980", L"If the road stays clear, we can still make it.", L"Jestli z\u016Fstane cesta voln\u00E1, po\u0159\u00E1d to stihneme.", L"Schv\u00E1leno" },
            { 145, L"00:12:44.120", L"00:12:46.840", 2.72, L"00:12:44.120", L"00:12:46.840", L"We should be there before sunrise.", L"M\u011Bli bychom tam b\u00FDt p\u0159ed v\u00FDchodem slunce.", L"Upraveno" },
            { 146, L"00:12:47.050", L"00:12:49.300", 2.25, L"00:12:47.050", L"00:12:49.300", L"Then we wait for the signal.", L"Pak po\u010Dk\u00E1me na sign\u00E1l.", L"P\u0159ipraveno" },
            { 147, L"00:12:50.100", L"00:12:52.650", 2.55, L"00:12:50.100", L"00:12:52.650", L"No mistakes this time.", L"Tentokr\u00E1t bez chyb.", L"P\u0159ipraveno" },
        };

        std::vector<SubtitleEntry> m_sourceEntries;
        std::vector<SubtitleEntry> m_targetEntries;
        std::vector<winrt::Microsoft::UI::Xaml::Controls::Border> m_rowBorders;
        std::vector<winrt::Microsoft::UI::Xaml::Controls::TextBlock> m_rowTargetTexts;
        std::vector<winrt::Microsoft::UI::Xaml::Controls::TextBlock> m_rowStatusTexts;
        winrt::Microsoft::UI::Xaml::Controls::Grid m_subtitleGrid{ nullptr };
        winrt::hstring m_sourcePath;
        winrt::hstring m_targetPath;

        int32_t m_currentIndex{ 2 };
        bool m_loadingSelection{ false };
        bool m_initialized{ false };

        void LoadCurrentRow();
        void UpdateMetrics();
        void UpdateSelectionVisuals();
        void UpdateTableRow(int32_t index);
        int32_t RowIndexFromSender(winrt::Windows::Foundation::IInspectable const& sender) const;

        void InitializeDynamicSubtitleGrid();
        void RebuildSubtitleGrid();
        void HookOpenProjectButton();
        void OpenProjectFiles();
        bool SelectSubtitleFile(std::wstring const& title, std::wstring& filename) const;
        bool ReadSubtitleFile(
            std::wstring const& filename,
            std::vector<SubtitleEntry>& entries,
            std::wstring& errorMessage) const;
        void BuildAlignedRows();
        bool SaveTargetSubtitleFile(std::wstring& errorMessage);

        winrt::Microsoft::UI::Xaml::Controls::Button FindOpenProjectButton(
            winrt::Microsoft::UI::Xaml::DependencyObject const& root) const;
    };
}

namespace winrt::Aegisub_WinUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
