#pragma once

#include "AbstractThemeApplier.h"

namespace HomeCompa::Flibrary
{

class PluginStyleApplier : public AbstractThemeApplier
{
public:
	explicit PluginStyleApplier(std::shared_ptr<ISettings> settings);

private:
	Type GetType() const noexcept override;
	std::unique_ptr<Util::DyLib> Set(QApplication& app) const override;
};

}
