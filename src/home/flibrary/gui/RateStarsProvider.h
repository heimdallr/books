#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"
#include "interface/ui/IRateStarsProvider.h"

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Flibrary {

class RateStarsProvider final
	: virtual public IRateStarsProvider
{
	NON_COPY_MOVABLE(RateStarsProvider)

public:
	explicit RateStarsProvider(std::shared_ptr<ISettings> settings);
	~RateStarsProvider() override;

private: // IRateStarsProvider
	QPixmap GetStars(int rate) const override;
	QSize GetSize(int rate) const override;
	void Render(QPainter * painter, const QRect & rect, int rate) const override;

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};

}
