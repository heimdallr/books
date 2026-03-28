#pragma once

#include "fnd/NonCopyMovable.h"

#include "AbstractThemeApplier.h"

namespace HomeCompa::Flibrary
{

class PluginStyleApplier final : public AbstractThemeApplier
{
	NON_COPY_MOVABLE(PluginStyleApplier)

public:
	explicit PluginStyleApplier(std::shared_ptr<ISettings> settings);
	~PluginStyleApplier() override;

private:
	Type                             GetType() const noexcept override;
	std::unique_ptr<Platform::DyLib> Set(QApplication& app) const override;
};

}
