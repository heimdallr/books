#include "DllStyleApplier.h"

#include <QApplication>

#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"

using namespace HomeCompa;
using namespace Flibrary;

DllStyleApplier::DllStyleApplier(std::shared_ptr<ISettings> settings)
	: AbstractThemeApplier(std::move(settings))
{
}

IStyleApplier::Type DllStyleApplier::GetType() const noexcept
{
	return Type::DllStyle;
}

std::unique_ptr<Util::DyLib> DllStyleApplier::Set(QApplication& app) const
{
	const auto fileName = m_settings->Get(Constant::Settings::THEME_FILE_KEY).toString();
	auto result = std::make_unique<Util::DyLib>(fileName.toStdString());
	if (!result->IsOpen())
		return result;

	const auto qssName = m_settings->Get(Constant::Settings::THEME_NAME_KEY).toString();
	auto stylesheet = ReadStyleSheet(qssName);
	stylesheet.append(ReadStyleSheet(Constant::STYLE_FILE_NAME));
	app.setStyleSheet(stylesheet);

	return result;
}
