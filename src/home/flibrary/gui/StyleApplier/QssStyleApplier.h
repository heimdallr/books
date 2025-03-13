#pragma once

#include "fnd/NonCopyMovable.h"
#include "AbstractThemeApplier.h"

namespace HomeCompa::Flibrary
{

class QssStyleApplier : public AbstractThemeApplier
{
	NON_COPY_MOVABLE(QssStyleApplier)

public:
	explicit QssStyleApplier(std::shared_ptr<ISettings> settings);
	~QssStyleApplier() override;

private:
	Type GetType() const noexcept override;
	std::unique_ptr<Util::DyLib> Set(QApplication& app) const override;
};

}
