# Kontrola vydání Aegisub Translation Workspace

## Sestavení

1. Sestavte `aegisub-winui-bridge.exe` v hlavním Meson/Ninja buildu.
2. Sestavte řešení `Aegisub.WinUI.slnx` v konfiguraci `Release | x64`.
3. Vytvořte nepodepsaný MSIX přes cíl `Publish` nebo `GenerateAppxPackageOnBuild`.
4. Ověřte, že výstup obsahuje `Aegisub_WinUI.exe` i `aegisub-winui-bridge.exe`.

## Čistý počítač nebo virtuální stroj

1. Nainstalujte důvěryhodný podpisový certifikát a poté MSIX balíček.
2. Otevřete dvojici `.srt`, upravte text a ověřte `Ctrl+S`, `Ctrl+Z` a `Ctrl+Y`.
3. Ukončete aplikaci přes Správce úloh nejméně sekundu po změně textu.
4. Znovu otevřete stejný projekt a potvrďte obnovení pracovního konceptu.
5. Změňte český soubor mimo aplikaci a ověřte volby načíst, uložit jako a pokračovat.
6. Ověřte přehled, obnovení a odstranění obnovovacích dat v nabídce **Soubor**.
7. Ověřte češtinu, klávesovou navigaci, zobrazení při 100 % a 150 % DPI a minimální velikost okna.

Bez přístupu k druhému čistému systému je možné automaticky ověřit sestavení a obsah balíčku, nikoli instalaci certifikátu a chování konkrétního cílového Windows.
