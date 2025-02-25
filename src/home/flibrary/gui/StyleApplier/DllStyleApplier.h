#pragma once

#include "AbstractThemeApplier.h"

namespace HomeCompa::Flibrary
{

class DllStyleApplier : public AbstractThemeApplier
{
public:
	explicit DllStyleApplier(std::shared_ptr<ISettings> settings);

private:
	Type GetType() const noexcept override;
	std::unique_ptr<Util::DyLib> Set(QApplication& app) const override;
};

}
