#pragma once

class QString;

namespace HomeCompa::Flibrary
{

class IReaderController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IReaderController()                    = default;
	virtual void Read(long long id) const           = 0;
	virtual void ReadRandomBook(QString lang) const = 0;
};

}
