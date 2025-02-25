#include "PluginStyleApplier.h"

#include <QApplication>
#include <QStyleFactory>

#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"

using namespace HomeCompa;
using namespace Flibrary;

PluginStyleApplier::PluginStyleApplier(std::shared_ptr<ISettings> settings)
	: AbstractThemeApplier(std::move(settings))
{
}

IStyleApplier::Type PluginStyleApplier::GetType() const noexcept
{
	return Type::PluginStyle;
}

std::unique_ptr<Util::DyLib> PluginStyleApplier::Set(QApplication& app) const
{
	auto style = m_settings->Get(Constant::Settings::THEME_NAME_KEY, Constant::Settings::THEME_NAME_DEFAULT);
	if (!QStyleFactory::keys().contains(style, Qt::CaseInsensitive))
		style = Constant::Settings::THEME_NAME_DEFAULT;
	
	QApplication::setStyle(style);

	app.setStyleSheet(ReadStyleSheet(Constant::STYLE_FILE_NAME));

	return {};
}
