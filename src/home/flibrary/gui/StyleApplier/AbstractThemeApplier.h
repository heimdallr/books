#pragma once

#include "AbstractStyleApplier.h"

namespace HomeCompa::Flibrary
{

class AbstractThemeApplier : public AbstractStyleApplier
{
public:
	explicit AbstractThemeApplier(std::shared_ptr<ISettings> settings);

private: // IStyleApplier
	void Apply(const QString& name, const QString& file) override;
	std::pair<QString, QString> GetChecked() const override;

protected:
	static QString ReadStyleSheet(const QString& fileName);
};

}
