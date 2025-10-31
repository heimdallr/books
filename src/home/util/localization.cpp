#include "localization.h"

#include <ranges>

#include <QCoreApplication>
#include <QDir>
#include <QTranslator>

#include "ISettings.h"
#include "log.h"

#include "config/locales.h"

namespace HomeCompa::Loc
{

namespace
{
constexpr auto LOCALE = "ui/locale";
}

QString Tr(const char* context, const char* str)
{
	return QCoreApplication::translate(context, str);
}

std::vector<const char*> GetLocales()
{
	const QDir dir = QCoreApplication::applicationDirPath() + "/locales";

	std::vector<const char*> result;
	std::ranges::copy(
		LOCALES | std::views::filter([&](const auto* item) {
			return !dir.entryList(QStringList() << QString("*_%1.qm").arg(item), QDir::Files).isEmpty();
		}),
		std::back_inserter(result)
	);
	return result;
}

QString GetLocale(const ISettings& settings)
{
	if (auto locale = settings.Get(LOCALE).toString(); !locale.isEmpty())
		return locale;

	if (const auto it = std::ranges::find_if(
			LOCALES,
			[sysLocale = QLocale::system().name()](const char* item) {
				return sysLocale.startsWith(item);
			}
		);
	    it != std::cend(LOCALES))
		return *it;

	assert(!std::empty(Loc::LOCALES));
	return LOCALES[0];
}

std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const ISettings& settings)
{
	return LoadLocales(GetLocale(settings));
}

std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const QString& locale)
{
	std::vector<PropagateConstPtr<QTranslator>> translators;
	const QDir                                  dir = QCoreApplication::applicationDirPath() + "/locales";

	for (const auto& file : dir.entryList(QStringList() << QString("*_%1.qm").arg(locale), QDir::Files))
	{
		const auto fileName   = dir.absoluteFilePath(file);
		auto&      translator = translators.emplace_back();
		if (translator->load(fileName))
			QCoreApplication::installTranslator(translator.get());
		else
			PLOGE << "Cannot load " << fileName;
	}

	return translators;
}

} // namespace HomeCompa::Loc
