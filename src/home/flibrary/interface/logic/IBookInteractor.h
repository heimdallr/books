#pragma once

namespace HomeCompa::Flibrary
{

class IBookInteractor // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBookInteractor() = default;

public:
	virtual void OnLinkActivated(long long bookId) const = 0;
	virtual void OnDoubleClicked(long long bookId) const = 0;
};

}
