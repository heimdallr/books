#pragma once

#include "fnd/NonCopyMovable.h"

#include "AbstractStyleApplier.h"

namespace HomeCompa::Flibrary
{

class ColorSchemeApplier final : public AbstractStyleApplier
{
	NON_COPY_MOVABLE(ColorSchemeApplier)

public:
	explicit ColorSchemeApplier(std::shared_ptr<ISettings> settings);
	~ColorSchemeApplier() override;

private:
	Type GetType() const noexcept override;
	void Apply(const QString& name, const QString& file) override;
	std::pair<QString, QString> GetChecked() const override;
	std::unique_ptr<Util::DyLib> Set(QApplication&) const override;
};

}
