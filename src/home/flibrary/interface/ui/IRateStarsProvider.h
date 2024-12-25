#pragma once

class QPainter;
class QPixmap;
class QRect;
class QSize;

namespace HomeCompa::Flibrary {

class IRateStarsProvider  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IRateStarsProvider() = default;
	virtual QPixmap GetStars(int rate) const = 0;
	virtual QSize GetSize(int rate) const = 0;
	virtual void Render(QPainter * painter, const QRect & rect, int rate) const = 0;
};

}
