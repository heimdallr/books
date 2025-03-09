#include "PluginStyleApplier.h"

#include <QApplication>
#include <QStyleFactory>

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
	auto style = m_settings->Get(THEME_NAME_KEY, THEME_NAME_DEFAULT);
	if (!QStyleFactory::keys().contains(style, Qt::CaseInsensitive))
		style = THEME_NAME_DEFAULT;

	QApplication::setStyle(style);

	app.setStyleSheet(ReadStyleSheet(STYLE_FILE_NAME));

	return {};
}
