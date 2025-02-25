#pragma once

#include "AbstractThemeApplier.h"

namespace HomeCompa::Flibrary
{

class QssStyleApplier : public AbstractThemeApplier
{
public:
	explicit QssStyleApplier(std::shared_ptr<ISettings> settings);

private:
	Type GetType() const noexcept override;
	std::unique_ptr<Util::DyLib> Set(QApplication& app) const override;
};

}
