#include "JokeRequesterFactory.h"

#include <QString>

#include <Hypodermic/Container.h>

#include "fnd/FindPair.h"

#include "JokeRequester/CatFactJokeRequester.h"
#include "JokeRequester/CatPicsJokeRequester.h"
#include "JokeRequester/ChuckNorrisJokeRequester.h"
#include "JokeRequester/DadJokeRequester.h"
#include "JokeRequester/DogPicsJokeRequester.h"
#include "JokeRequester/FoxPicsJokeRequester.h"
#include "JokeRequester/JokeApiJokeRequester.h"
#include "JokeRequester/QuoteJokeRequester.h"
#include "JokeRequester/QuotePicJokeRequester.h"
#include "JokeRequester/SetupPunchlineJokeRequester.h"
#include "JokeRequester/UselessFactJokeRequester.h"
#include "network/network/downloader.h"
#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "JokeRequester";
constexpr auto CatFactJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "CatFacts");
constexpr auto CatPicsJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "CatPictures");
constexpr auto ChuckNorrisJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "ChuckNorrisFacts");
constexpr auto DadJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "DadJokes");
constexpr auto DogPicsJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "DogPictures");
constexpr auto FoxPicsJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "FoxPictures");
constexpr auto JokeApiJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "Jokes");
constexpr auto QuoteJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "Quotes");
constexpr auto QuotePicJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "QuotePics");
constexpr auto SetupPunchlineJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "PunchlineJokes");
constexpr auto UselessFactJokeRequesterTitle = QT_TRANSLATE_NOOP("JokeRequester", "UselessFacts");
constexpr auto CatFactJokeRequesterDisclaimer = "";
constexpr auto CatPicsJokeRequesterDisclaimer = "";
constexpr auto ChuckNorrisJokeRequesterDisclaimer = "";
constexpr auto DadJokeRequesterDisclaimer = "";
constexpr auto DogPicsJokeRequesterDisclaimer = "";
constexpr auto FoxPicsJokeRequesterDisclaimer = "";
constexpr auto JokeApiJokeRequesterDisclaimer = QT_TRANSLATE_NOOP(
	"JokeRequester",
	R"(<center><b>Warning!</b></center><br/></br>Jokes in this section may be rude, unsafe for work environment, religiously or politically offensive, sexist, or explicit. By clicking "Yes" you accept all responsibility for reading them. Otherwise you must click "No".<br/><br/>Do you still want to read such jokes?)");
constexpr auto QuoteJokeRequesterDisclaimer = "";
constexpr auto QuotePicJokeRequesterDisclaimer = "";
constexpr auto SetupPunchlineJokeRequesterDisclaimer = "";
constexpr auto UselessFactJokeRequesterDisclaimer = "";
TR_DEF

template <typename T>
std::shared_ptr<IJokeRequester> CreateImpl(Hypodermic::Container& container)
{
	return container.resolve<T>();
}

constexpr std::pair<IJokeRequesterFactory::Implementation, std::tuple<const char*, const char*, std::shared_ptr<IJokeRequester> (*)(Hypodermic::Container&)>> IMPLEMENTATIONS[] {
#define JOKE_REQUESTER_IMPL_ITEM(NAME)                                                                \
	{                                                                                                 \
		IJokeRequesterFactory::Implementation::NAME,                                                  \
		{ NAME##JokeRequesterTitle, NAME##JokeRequesterDisclaimer, &CreateImpl<NAME##JokeRequester> } \
},
	JOKE_REQUESTER_IMPL_ITEMS_X_MACRO
#undef JOKE_REQUESTER_IMPL_ITEM
};

} // namespace

struct JokeRequesterFactory::Impl
{
	Hypodermic::Container& container;
	std::shared_ptr<Network::Downloader> downloader;
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
	std::ranges::transform(
		IMPLEMENTATIONS,
		std::back_inserter(result),
		[](const auto& item)
		{ return ImplementationDescription { .implementation = item.first, .name = std::get<0>(item.second), .title = Tr(std::get<0>(item.second)), .disclaimer = Tr(std::get<1>(item.second)) }; });
	return result;
}

std::shared_ptr<IJokeRequester> JokeRequesterFactory::Create(const Implementation impl) const
{
	return std::get<2>(FindSecond(IMPLEMENTATIONS, impl))(m_impl->container);
}

std::shared_ptr<Network::Downloader> JokeRequesterFactory::GetDownloader() const
{
	if (!m_impl->downloader)
		m_impl->downloader = std::make_shared<Network::Downloader>();
	return m_impl->downloader;
}
