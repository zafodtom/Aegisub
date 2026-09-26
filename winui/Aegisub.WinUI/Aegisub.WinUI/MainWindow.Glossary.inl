#pragma once

#include <fstream>

namespace winrt::Aegisub_WinUI::implementation
{
    inline std::wstring GlossaryLower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return value;
    }

    inline bool GlossaryContainsInsensitive(
        std::wstring const& haystack,
        std::wstring const& needle)
    {
        if (needle.empty())
            return false;
        return GlossaryLower(haystack).find(GlossaryLower(needle)) != std::wstring::npos;
    }

    inline std::filesystem::path GlossaryLastPathPointer()
    {
        auto root = WinUiLocalAppDataPath();
        if (root.empty())
            return {};
        return root / L"Aegisub" / L"translation-manual.last";
    }

    inline std::filesystem::path GlossaryDefaultPath()
    {
        auto root = WinUiLocalAppDataPath();
        if (root.empty())
            return {};
        return root / L"Aegisub" / L"translation-manual.tsv";
    }

    inline std::string GlossaryCleanField(winrt::hstring const& value)
    {
        auto text = to_string(value);
        for (auto& c : text)
        {
            if (c == '\t' || c == '\r' || c == '\n')
                c = ' ';
        }
        return text;
    }

    inline bool MainWindow::SelectGlossaryOpenFile(std::wstring& filename) const
    {
        wchar_t buffer[32768]{};
        wchar_t const filter[] =
            L"Překladatelský manuál (*.tsv;*.txt)\0*.tsv;*.txt\0"
            L"Všechny soubory (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrTitle = L"Importovat překladatelský manuál";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&dialog))
            return false;

        filename = buffer;
        return true;
    }

    inline bool MainWindow::SelectGlossarySaveFile(std::wstring& filename) const
    {
        wchar_t buffer[32768]{};
        if (!m_glossaryPath.empty())
            wcsncpy_s(buffer, m_glossaryPath.c_str(), _TRUNCATE);
        else
            wcsncpy_s(buffer, L"translation-manual.tsv", _TRUNCATE);

        wchar_t const filter[] =
            L"Překladatelský manuál (*.tsv)\0*.tsv\0"
            L"Text (*.txt)\0*.txt\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrDefExt = L"tsv";
        dialog.lpstrTitle = L"Exportovat překladatelský manuál";
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&dialog))
            return false;

        filename = buffer;
        return true;
    }

    inline void MainWindow::RememberGlossaryPath() const
    {
        if (m_glossaryPath.empty())
            return;

        auto const pointer = GlossaryLastPathPointer();
        if (pointer.empty())
            return;

        std::error_code error;
        std::filesystem::create_directories(pointer.parent_path(), error);
        if (error)
            return;

        std::ofstream stream(pointer, std::ios::binary | std::ios::trunc);
        if (stream)
            stream << to_string(winrt::hstring{ m_glossaryPath });
    }

    inline void MainWindow::EnsureGlossaryAutoSavePath()
    {
        if (!m_glossaryPath.empty())
            return;

        auto const path = GlossaryDefaultPath();
        if (path.empty())
            return;

        m_glossaryPath = path.wstring();
        RememberGlossaryPath();
        GlossaryFileText().Text(winrt::hstring{ path.filename().wstring() });
    }

    inline bool MainWindow::SaveGlossaryToFile(std::wstring const& path)
    {
        if (path.empty())
            return false;

        std::error_code error;
        auto const filePath = std::filesystem::path{ path };
        if (!filePath.parent_path().empty())
            std::filesystem::create_directories(filePath.parent_path(), error);

        std::ofstream stream(filePath, std::ios::binary | std::ios::trunc);
        if (!stream)
            return false;

        for (auto const& entry : m_glossaryEntries)
        {
            if (entry.source.empty() && entry.target.empty())
                continue;
            stream << GlossaryCleanField(entry.source) << '\t'
                   << GlossaryCleanField(entry.target) << '\n';
        }
        return static_cast<bool>(stream);
    }

    inline bool MainWindow::LoadGlossaryFromFile(std::wstring const& path)
    {
        std::ifstream stream(std::filesystem::path{ path }, std::ios::binary);
        if (!stream)
            return false;

        std::vector<GlossaryEntry> entries;
        std::string line;
        bool firstLine = true;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (firstLine && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF)
            {
                line.erase(0, 3);
            }
            firstLine = false;

            if (line.empty())
                continue;

            auto const tab = line.find('\t');
            std::string source = tab == std::string::npos ? line : line.substr(0, tab);
            std::string target = tab == std::string::npos ? std::string{} : line.substr(tab + 1);
            entries.push_back({ to_hstring(source), to_hstring(target) });
        }

        m_glossaryEntries = std::move(entries);
        m_glossaryPath = std::filesystem::absolute(std::filesystem::path{ path }).wstring();
        RememberGlossaryPath();
        GlossaryFileText().Text(winrt::hstring{
            std::filesystem::path{ m_glossaryPath }.filename().wstring() });
        RebuildGlossaryGrid();
        return true;
    }

    inline void MainWindow::LoadLastGlossaryFile()
    {
        auto const pointer = GlossaryLastPathPointer();
        if (pointer.empty())
        {
            RebuildGlossaryGrid();
            return;
        }

        std::ifstream stream(pointer, std::ios::binary);
        std::string utf8;
        std::getline(stream, utf8);
        if (!utf8.empty())
        {
            auto const path = std::wstring{ to_hstring(utf8).c_str() };
            if (std::filesystem::exists(path) && LoadGlossaryFromFile(path))
                return;
        }

        RebuildGlossaryGrid();
    }

    inline void MainWindow::RefreshGlossaryForCurrentSubtitle()
    {
        RebuildGlossaryGrid();
    }

    inline void MainWindow::RebuildGlossaryGrid()
    {
        auto const grid = GlossaryGridHost();
        grid.Children().Clear();
        grid.RowDefinitions().Clear();

        std::wstring currentOriginal;
        if (!m_rows.empty() && m_currentIndex >= 0 &&
            m_currentIndex < static_cast<int32_t>(m_rows.size()))
        {
            currentOriginal = m_rows[m_currentIndex].original.c_str();
        }

        std::vector<size_t> order(m_glossaryEntries.size());
        std::iota(order.begin(), order.end(), size_t{ 0 });
        std::stable_sort(order.begin(), order.end(),
            [this, &currentOriginal](size_t a, size_t b)
            {
                auto const aSource = std::wstring{ m_glossaryEntries[a].source.c_str() };
                auto const bSource = std::wstring{ m_glossaryEntries[b].source.c_str() };
                bool const aMatch = GlossaryContainsInsensitive(currentOriginal, aSource);
                bool const bMatch = GlossaryContainsInsensitive(currentOriginal, bSource);
                if (aMatch != bMatch)
                    return aMatch > bMatch;
                return GlossaryLower(aSource) < GlossaryLower(bSource);
            });

        m_glossaryRebuilding = true;

        for (size_t visual = 0; visual < order.size(); ++visual)
        {
            auto const index = order[visual];

            winrt::Microsoft::UI::Xaml::Controls::RowDefinition row;
            row.Height(winrt::Microsoft::UI::Xaml::GridLength{
                30.0, winrt::Microsoft::UI::Xaml::GridUnitType::Pixel });
            grid.RowDefinitions().Append(row);

            auto const sourceText = std::wstring{ m_glossaryEntries[index].source.c_str() };
            bool const matched = GlossaryContainsInsensitive(currentOriginal, sourceText);

            winrt::Microsoft::UI::Xaml::Controls::TextBox sourceBox;
            sourceBox.Text(m_glossaryEntries[index].source);
            sourceBox.FontSize(11.0);
            sourceBox.Padding(winrt::Microsoft::UI::Xaml::Thickness{ 5.0, 2.0, 5.0, 2.0 });
            if (matched)
                sourceBox.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            winrt::Microsoft::UI::Xaml::Controls::Grid::SetRow(sourceBox, static_cast<int32_t>(visual));
            winrt::Microsoft::UI::Xaml::Controls::Grid::SetColumn(sourceBox, 0);

            winrt::Microsoft::UI::Xaml::Controls::TextBox targetBox;
            targetBox.Text(m_glossaryEntries[index].target);
            targetBox.FontSize(11.0);
            targetBox.Padding(winrt::Microsoft::UI::Xaml::Thickness{ 5.0, 2.0, 5.0, 2.0 });
            if (matched)
                targetBox.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            winrt::Microsoft::UI::Xaml::Controls::Grid::SetRow(targetBox, static_cast<int32_t>(visual));
            winrt::Microsoft::UI::Xaml::Controls::Grid::SetColumn(targetBox, 1);

            sourceBox.TextChanged([this, index](auto const& sender, auto const&)
            {
                if (m_glossaryRebuilding || index >= m_glossaryEntries.size())
                    return;
                m_glossaryEntries[index].source = sender.Text();
                EnsureGlossaryAutoSavePath();
                SaveGlossaryToFile(m_glossaryPath);
            });
            targetBox.TextChanged([this, index](auto const& sender, auto const&)
            {
                if (m_glossaryRebuilding || index >= m_glossaryEntries.size())
                    return;
                m_glossaryEntries[index].target = sender.Text();
                EnsureGlossaryAutoSavePath();
                SaveGlossaryToFile(m_glossaryPath);
            });

            auto reorderOnLeave = [this](auto const&, auto const&)
            {
                if (m_glossaryRebuilding)
                    return;

                m_glossaryEntries.erase(
                    std::remove_if(
                        m_glossaryEntries.begin(),
                        m_glossaryEntries.end(),
                        [](auto const& entry)
                        {
                            return entry.source.empty() && entry.target.empty();
                        }),
                    m_glossaryEntries.end());

                if (!m_glossaryPath.empty())
                    SaveGlossaryToFile(m_glossaryPath);
                RebuildGlossaryGrid();
            };
            sourceBox.LostFocus(reorderOnLeave);
            targetBox.LostFocus(reorderOnLeave);

            grid.Children().Append(sourceBox);
            grid.Children().Append(targetBox);
        }

        m_glossaryRebuilding = false;
    }

    inline void MainWindow::GlossaryAddButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        m_glossaryEntries.push_back({ L"", L"" });
        EnsureGlossaryAutoSavePath();
        RebuildGlossaryGrid();
        StatusBarText().Text(L"Přidán nový řádek překladatelského manuálu");
    }

    inline void MainWindow::GlossaryImportButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        std::wstring path;
        if (!SelectGlossaryOpenFile(path))
            return;

        if (!LoadGlossaryFromFile(path))
        {
            MessageBoxW(GetActiveWindow(), L"Soubor překladatelského manuálu se nepodařilo načíst.",
                L"Import manuálu", MB_OK | MB_ICONERROR);
            return;
        }

        StatusBarText().Text(L"Překladatelský manuál načten");
    }

    inline void MainWindow::GlossaryExportButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        std::wstring path;
        if (!SelectGlossarySaveFile(path))
            return;

        if (!SaveGlossaryToFile(path))
        {
            MessageBoxW(GetActiveWindow(), L"Překladatelský manuál se nepodařilo uložit.",
                L"Export manuálu", MB_OK | MB_ICONERROR);
            return;
        }

        m_glossaryPath = std::filesystem::absolute(std::filesystem::path{ path }).wstring();
        RememberGlossaryPath();
        GlossaryFileText().Text(winrt::hstring{
            std::filesystem::path{ m_glossaryPath }.filename().wstring() });
        StatusBarText().Text(L"Překladatelský manuál exportován");
    }
}
