#include "AbstractThemeApplier.h"

#include <QFile>

#include "interface/constants/SettingsConstant.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

AbstractThemeApplier::AbstractThemeApplier(std::shared_ptr<ISettings> settings)
	: AbstractStyleApplier(std::move(settings))
{
}

void AbstractThemeApplier::Apply(const QString& name, const QString& file)
{
	m_settings->Set(Constant::Settings::THEME_TYPE_KEY, TypeToString(GetType()));
	m_settings->Set(Constant::Settings::THEME_NAME_KEY, name);
	m_settings->Set(Constant::Settings::THEME_FILE_KEY, file);
}

std::pair<QString, QString> AbstractThemeApplier::GetChecked() const
{
	const auto currentType = m_settings->Get(Constant::Settings::THEME_TYPE_KEY, Constant::Settings::THEME_KEY_DEFAULT);
	const auto thisType = TypeToString(GetType());
	return currentType == thisType ? std::make_pair(m_settings->Get(Constant::Settings::THEME_NAME_KEY, Constant::Settings::THEME_NAME_DEFAULT), m_settings->Get(Constant::Settings::THEME_FILE_KEY).toString())
	                               : std::pair<QString, QString> {};
}

QString AbstractThemeApplier::ReadStyleSheet(const QString& fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		PLOGE << "Cannot open " << fileName;
		return {};
	}

	QTextStream ts(&file);
	return ts.readAll();
}
