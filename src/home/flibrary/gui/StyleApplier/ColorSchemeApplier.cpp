#include "ColorSchemeApplier.h"

#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

#include "fnd/FindPair.h"

#include "interface/constants/ProductConstant.h"

#include "Constant.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

ColorSchemeApplier::ColorSchemeApplier(std::shared_ptr<ISettings> settings)
	: AbstractStyleApplier(std::move(settings))
{
	PLOGV << "ColorSchemeApplier created";
}

ColorSchemeApplier ::~ColorSchemeApplier()
{
	PLOGV << "ColorSchemeApplier destroyed";
}

IStyleApplier::Type ColorSchemeApplier::GetType() const noexcept
{
	return Type::ColorScheme;
}

void ColorSchemeApplier::Apply(const QString& name, const QString& /*data*/)
{
	m_settings->Set(COLOR_SCHEME_KEY, name);
}

std::pair<QString, QString> ColorSchemeApplier::GetChecked() const
{
	return std::make_pair(m_settings->Get(COLOR_SCHEME_KEY, APP_COLOR_SCHEME_DEFAULT), QString {});
}

std::unique_ptr<Util::DyLib> ColorSchemeApplier::Set(QApplication&) const
{
	using Scheme = std::tuple<Qt::ColorScheme, const char*>;
	constexpr Scheme                         unknown { Qt::ColorScheme::Unknown, nullptr };
	constexpr auto                           iconsLight = "icolight";
	constexpr auto                           iconsDark  = "icodark";
	constexpr std::pair<const char*, Scheme> schemes[] {
		{ "System",								unknown },
		{  "Light", { Qt::ColorScheme::Light, iconsLight } },
		{   "Dark",   { Qt::ColorScheme::Dark, iconsDark } },
	};

	const auto colorSchemeName = m_settings->Get(COLOR_SCHEME_KEY, APP_COLOR_SCHEME_DEFAULT);
	auto [scheme, iconSet]     = FindSecond(schemes, colorSchemeName.toStdString().data(), unknown, PszComparer {});

	if (m_settings->Get(THEME_TYPE_KEY, THEME_KEY_DEFAULT) == TypeToString(Type::PluginStyle))
	{
		QGuiApplication::styleHints()->setColorScheme(scheme);

		if (scheme == Qt::ColorScheme::Unknown)
			QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, [] {
				QCoreApplication::exit(Global::RESTART_APP);
			});
		else
			QGuiApplication::styleHints()->disconnect();
	}

	if (!iconSet)
	{
		const auto palette         = QGuiApplication::palette();
		const auto textLightness   = palette.color(QPalette::WindowText).lightness();
		const auto windowLightness = palette.color(QPalette::Window).lightness();
		iconSet                    = textLightness > windowLightness ? iconsDark : iconsLight;
	}

	return std::make_unique<Util::DyLib>(iconSet);
}
