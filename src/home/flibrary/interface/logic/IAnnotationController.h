#pragma once

class QString;

namespace HomeCompa::Flibrary {

class IAnnotationController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IAnnotationController() = default;

public:
	virtual void SetCurrentBookId(QString bookId) = 0;
};

}
