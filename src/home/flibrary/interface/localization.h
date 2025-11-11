#pragma once

#include <QString>

#include "fnd/memory.h"

#include "interface/constants/Localization.h"

#include <export/flint.h>

class QTranslator;

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Loc
{

FLINT_EXPORT QString Tr(const char* context, const char* str);
FLINT_EXPORT std::vector<const char*> GetLocales();
FLINT_EXPORT QString                  GetLocale(const ISettings& settings);
FLINT_EXPORT std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const ISettings& settings);
FLINT_EXPORT std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const QString& locale);

inline QString Error()
{
	return Tr(Ctx::COMMON, ERROR_TEXT);
}

inline QString Information()
{
	return Tr(Ctx::COMMON, INFORMATION);
}

inline QString Question()
{
	return Tr(Ctx::COMMON, QUESTION);
}

inline QString Warning()
{
	return Tr(Ctx::COMMON, WARNING);
}

} // namespace HomeCompa::Loc

#define TR_DEF                         \
	inline QString Tr(const char* str) \
	{                                  \
		return Loc::Tr(CONTEXT, str);  \
	}
