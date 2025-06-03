#include "JokeRequesterFactory.h"

#include <QString>

#include <Hypodermic/Container.h>

#include "fnd/FindPair.h"

#include "util/Localization.h"

#include "SetupPunchlineJokeRequester.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "JokeRequester";
constexpr auto SetupPunchlineJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "Punchline");
TR_DEF

template <typename T>
std::shared_ptr<IJokeRequester> CreateImpl(Hypodermic::Container& container)
{
	return container.resolve<T>();
}

constexpr std::pair<IJokeRequesterFactory::Implementation, std::tuple<const char*, std::shared_ptr<IJokeRequester> (*)(Hypodermic::Container&)>> IMPLEMENTATIONS[] {
#define JOKE_REQUESTER_IMPL_ITEM(NAME)                                 \
	{                                                                  \
		IJokeRequesterFactory::Implementation::NAME,                   \
		{ NAME##JokeRequesterTitle, &CreateImpl<NAME##JokeRequester> } \
},
	JOKE_REQUESTER_IMPL_ITEMS_X_MACRO
#undef JOKE_REQUESTER_IMPL_ITEM
};

}

struct JokeRequesterFactory::Impl
{
	Hypodermic::Container& container;
};

JokeRequesterFactory::JokeRequesterFactory(Hypodermic::Container& container)
	: m_impl { std::make_unique<Impl>(container) }
{
}

JokeRequesterFactory::~JokeRequesterFactory() = default;

std::vector<IJokeRequesterFactory::ImplementationDescription> JokeRequesterFactory::GetImplementations() const
{
	std::vector<ImplementationDescription> result;
	result.reserve(std::size(IMPLEMENTATIONS));
	std::ranges::transform(IMPLEMENTATIONS,
	                       std::back_inserter(result),
	                       [](const auto& item) { return ImplementationDescription { .implementation = item.first, .name = std::get<0>(item.second), .title = Tr(std::get<0>(item.second)) }; });
	return result;
}

std::shared_ptr<IJokeRequester> JokeRequesterFactory::Create(const Implementation impl) const
{
	return std::get<1>(FindSecond(IMPLEMENTATIONS, impl))(m_impl->container);
}
