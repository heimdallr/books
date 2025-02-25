#pragma once

#include "AbstractStyleApplier.h"

namespace HomeCompa::Flibrary
{

class ColorSchemeApplier : public AbstractStyleApplier
{
public:
	explicit ColorSchemeApplier(std::shared_ptr<ISettings> settings);

private:
	Type GetType() const noexcept override;
	void Apply(const QString& name, const QVariant& data) override;
	std::pair<QString, QVariant> GetChecked() const override;
};

}
