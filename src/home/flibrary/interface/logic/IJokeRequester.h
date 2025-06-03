#pragma once

class QString;

namespace HomeCompa::Flibrary
{

class IJokeRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IClient // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IClient() = default;
		virtual void OnTextReceived(const QString& value) = 0;
	};

public:
	virtual ~IJokeRequester() = default;
	virtual void Request(std::weak_ptr<IClient> client) = 0;
};

} // namespace HomeCompa::Flibrary
