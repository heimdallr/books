#include "ColorSchemeApplier.h"

#include "interface/constants/SettingsConstant.h"

using namespace HomeCompa::Flibrary;

ColorSchemeApplier::ColorSchemeApplier(std::shared_ptr<ISettings> settings)
	: AbstractStyleApplier(std::move(settings))
{
}

IStyleApplier::Type ColorSchemeApplier::GetType() const noexcept
{
	return Type::ColorScheme;
}

void ColorSchemeApplier::Apply(const QString& name, const QVariant& /*data*/)
{
	m_settings->Set(Constant::Settings::COLOR_SCHEME_KEY, name);
}

std::pair<QString, QVariant> ColorSchemeApplier::GetChecked() const
{
	return std::make_pair(m_settings->Get(Constant::Settings::COLOR_SCHEME_KEY, Constant::Settings::APP_COLOR_SCHEME_DEFAULT), QVariant {});
}
