#include "PluginStyleApplier.h"

#include "interface/constants/SettingsConstant.h"

using namespace HomeCompa::Flibrary;

PluginStyleApplier::PluginStyleApplier(std::shared_ptr<ISettings> settings)
	: AbstractStyleApplier(std::move(settings))
{
}

IStyleApplier::Type PluginStyleApplier::GetType() const noexcept
{
	return Type::PluginStyle;
}

void PluginStyleApplier::Apply(const QString& name, const QVariant& /*data*/)
{
	m_settings->Set(Constant::Settings::COLOR_SCHEME_KEY, name);
}

std::pair<QString, QVariant> PluginStyleApplier::GetChecked() const
{
	return std::make_pair(m_settings->Get(Constant::Settings::COLOR_SCHEME_KEY, Constant::Settings::APP_COLOR_SCHEME_DEFAULT), QVariant {});
}
