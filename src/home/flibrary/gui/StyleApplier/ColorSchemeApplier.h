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
	void Apply(const QString& name, const QString& file) override;
	std::pair<QString, QString> GetChecked() const override;
	std::unique_ptr<Util::DyLib> Set(QApplication&) const override;
};

}
