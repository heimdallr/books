#pragma once

#include <memory>
#include <vector>

#include <QString>

namespace HomeCompa::Flibrary
{

#define JOKE_REQUESTER_IMPL_ITEMS_X_MACRO    \
	JOKE_REQUESTER_IMPL_ITEM(CatFact)        \
	JOKE_REQUESTER_IMPL_ITEM(ChuckNorris)    \
	JOKE_REQUESTER_IMPL_ITEM(SetupPunchline) \
	JOKE_REQUESTER_IMPL_ITEM(JokeApi)        \
	JOKE_REQUESTER_IMPL_ITEM(DogPics)        \
	JOKE_REQUESTER_IMPL_ITEM(FoxPics)

class IJokeRequesterFactory // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	enum class Implementation
	{
#define JOKE_REQUESTER_IMPL_ITEM(NAME) NAME,
		JOKE_REQUESTER_IMPL_ITEMS_X_MACRO
#undef JOKE_REQUESTER_IMPL_ITEM
	};

	struct ImplementationDescription
	{
		Implementation implementation;
		QString name;
		QString title;
	};

public:
	virtual ~IJokeRequesterFactory() = default;
	[[nodiscard]] virtual std::vector<ImplementationDescription> GetImplementations() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IJokeRequester> Create(Implementation impl) const = 0;
};

} // namespace HomeCompa::Flibrary
