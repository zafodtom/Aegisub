#pragma once

#include <clocale>

namespace agi::winui {

// The bulk translation workflow uses the C wide-character classification
// functions for case-insensitive matching and consistency checks. The default
// "C" locale only guarantees ASCII case conversion, so initialize LC_CTYPE to
// a Unicode-capable locale before the WinUI shell starts processing subtitles.
inline bool InitializeTextLocale() {
#ifdef _WIN32
	if (std::setlocale(LC_CTYPE, ".UTF-8"))
		return true;
#else
	// CI and development shells often inherit LANG=C even though a UTF-8 locale
	// is installed. Try common portable names before consulting the environment.
	if (std::setlocale(LC_CTYPE, "C.UTF-8"))
		return true;
	if (std::setlocale(LC_CTYPE, "C.utf8"))
		return true;
	if (std::setlocale(LC_CTYPE, "en_US.UTF-8"))
		return true;
	if (std::setlocale(LC_CTYPE, "UTF-8"))
		return true;
#endif
	return std::setlocale(LC_CTYPE, "") != nullptr;
}

}
