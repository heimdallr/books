#include "KeyboardLayout.h"

#include <Windows.h>

#include "fnd/FindPair.h"

namespace HomeCompa {

namespace {

constexpr std::pair<const char *, const wchar_t*> LOCALES[]
{
	{"en", L"00000409"},
	{"ru", L"00000419"},
};

}

void SetKeyboardLayout(const std::string & locale)
{
	const auto * keyboardLayoutId = FindSecond(LOCALES, locale.data(), PszComparer {});
	LoadKeyboardLayout(keyboardLayoutId, KLF_ACTIVATE);
}

}
