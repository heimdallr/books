#pragma once

class QString;

namespace HomeCompa::Flibrary
{

class IReaderController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Callback = std::function<void()>;

public:
	virtual ~IReaderController() = default;
	virtual void Read(const QString& folderName, QString fileName, Callback callback) const = 0;
	virtual void Read(long long id) const = 0;
	virtual void ReadRandomBook(QString lang) const = 0;
};

}
