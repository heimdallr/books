#pragma once

#include "fnd/NonCopyMovable.h"

#include "AbstractThemeApplier.h"

namespace HomeCompa::Flibrary
{

class DllStyleApplier final : public AbstractThemeApplier
{
	NON_COPY_MOVABLE(DllStyleApplier)

public:
	explicit DllStyleApplier(std::shared_ptr<ISettings> settings);
	~DllStyleApplier() override;

private:
	Type                         GetType() const noexcept override;
	std::unique_ptr<Util::DyLib> Set(QApplication& app) const override;
};

}
