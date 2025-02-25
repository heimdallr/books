#include "QssStyleApplier.h"

#include <QApplication>

#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"

using namespace HomeCompa;
using namespace Flibrary;

QssStyleApplier::QssStyleApplier(std::shared_ptr<ISettings> settings)
	: AbstractThemeApplier(std::move(settings))
{
}

IStyleApplier::Type QssStyleApplier::GetType() const noexcept
{
	return Type::QssStyle;
}

std::unique_ptr<Util::DyLib> QssStyleApplier::Set(QApplication& app) const
{
	const auto fileName = m_settings->Get(Constant::Settings::THEME_FILE_KEY).toString();

	auto stylesheet = ReadStyleSheet(fileName);
	stylesheet.append(ReadStyleSheet(Constant::STYLE_FILE_NAME));
	app.setStyleSheet(stylesheet);

	return {};
}
