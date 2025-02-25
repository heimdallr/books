#pragma once

#include "AbstractStyleApplier.h"

namespace HomeCompa::Flibrary
{

class PluginStyleApplier : public AbstractStyleApplier
{
public:
	explicit PluginStyleApplier(std::shared_ptr<ISettings> settings);

private:
	Type GetType() const noexcept override;
	void Apply(const QString& name, const QVariant& data) override;
	std::pair<QString, QVariant> GetChecked() const override;
};

}
