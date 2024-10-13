#include "localization.h"

#include <QCoreApplication>
#include <QDir>
#include <QTranslator>

#include <plog/Log.h>

#include "ISettings.h"

namespace HomeCompa::Loc {

namespace {
constexpr auto LOCALE = "ui/locale";
}

QString Tr(const char * context, const char * str)
{
	return QCoreApplication::translate(context, str);
}

QString GetLocale(const ISettings & settings)
{
	if (auto locale = settings.Get(LOCALE).toString(); !locale.isEmpty())
		return locale;

	if (const auto it = std::ranges::find_if(Loc::LOCALES, [sysLocale = QLocale::system().name()] (const char * item)
	{
		return sysLocale.startsWith(item);
	}); it != std::cend(Loc::LOCALES))
		return *it;

	assert(!std::empty(Loc::LOCALES));
	return Loc::LOCALES[0];
}

std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const ISettings & settings)
{
	std::vector<PropagateConstPtr<QTranslator>> translators;
	const QDir dir = QCoreApplication::applicationDirPath() + "/locales";

	for (const auto & file : dir.entryList(QStringList() << QString("*_%1.qm").arg(GetLocale(settings)), QDir::Files))
	{
		const auto fileName = dir.absoluteFilePath(file);
		auto & translator = translators.emplace_back();
		if (translator->load(fileName))
			QCoreApplication::installTranslator(translator.get());
		else
			PLOGE << "Cannot load " << fileName;
	}

	return translators;
}

}
