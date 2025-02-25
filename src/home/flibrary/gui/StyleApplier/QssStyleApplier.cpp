#include "QssStyleApplier.h"

#include <QApplication>

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
	const auto fileName = m_settings->Get(THEME_FILE_KEY).toString();

	auto stylesheet = ReadStyleSheet(fileName);
	stylesheet.append(ReadStyleSheet(STYLE_FILE_NAME));
	app.setStyleSheet(stylesheet);

	return {};
}
