#pragma once

class QPixmap;

namespace HomeCompa::Flibrary {

class IRateStarsProvider  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IRateStarsProvider() = default;
	virtual QPixmap GetStars(int rate) const = 0;
};

}
