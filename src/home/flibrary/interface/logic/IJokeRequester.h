#pragma once

class QString;
class QByteArray;

namespace HomeCompa::Flibrary
{

class IJokeRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IClient // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IClient() = default;

		virtual void OnError(const QString& api, const QString& error) = 0;
		virtual void OnTextReceived(const QString& value)              = 0;
		virtual void OnImageReceived(const QByteArray& value)          = 0;
	};

public:
	virtual ~IJokeRequester()                           = default;
	virtual void Request(std::weak_ptr<IClient> client) = 0;
};

} // namespace HomeCompa::Flibrary
