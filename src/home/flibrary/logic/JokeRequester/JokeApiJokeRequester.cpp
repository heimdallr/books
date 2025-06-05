#include "JokeApiJokeRequester.h"

#include <QJsonObject>

#include "fnd/FindPair.h"

#include "interface/logic/IJokeRequesterFactory.h"

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "JokeRequester";
constexpr auto CATEGORY = QT_TRANSLATE_NOOP("JokeRequester", "Category");
#if 0
QT_TRANSLATE_NOOP("JokeRequester", "Programming");
QT_TRANSLATE_NOOP("JokeRequester", "Misc");
QT_TRANSLATE_NOOP("JokeRequester", "Dark");
QT_TRANSLATE_NOOP("JokeRequester", "Pun");
QT_TRANSLATE_NOOP("JokeRequester", "Spooky");
QT_TRANSLATE_NOOP("JokeRequester", "Christmas");
#endif
TR_DEF

QString single(const QJsonObject& jsonObject)
{
	const auto category = jsonObject["category"], joke = jsonObject["joke"];
	return category.isString() && joke.isString() ? QString("<p>%1: %2</p><p>%3</p>").arg(Tr(CATEGORY), Tr(category.toString().toStdString().data()), joke.toString()) : QString {};
}

QString twopart(const QJsonObject& jsonObject)
{
	const auto category = jsonObject["category"], setup = jsonObject["setup"], delivery = jsonObject["delivery"];
	return category.isString() && setup.isString() && delivery.isString()
	         ? QString("<p>%1: %2</p><p>%3 %4</p><p>%3 %5</p>").arg(Tr(CATEGORY), Tr(category.toString().toStdString().data()), QChar(0x2014), setup.toString(), delivery.toString())
	         : QString {};
}

QString stub(const QJsonObject&)
{
	return {};
}

constexpr std::pair<const char*, QString (*)(const QJsonObject&)> PARSERS[] {
#define ITEM(NAME) { #NAME, &(NAME) }
	ITEM(single),
	ITEM(twopart),
#undef ITEM
};

} // namespace

JokeApiJokeRequester::JokeApiJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: BaseJokeRequester(jokeRequesterFactory->GetDownloader(), "http://v2.jokeapi.dev/joke/Any")
{
}

bool JokeApiJokeRequester::Process(const QJsonValue& value, std::weak_ptr<IClient> clientPtr)
{
	const auto client = clientPtr.lock();
	if (!client)
		return true;

	if (!value.isObject())
		return false;

	const auto jsonObject = value.toObject();
	const auto type = jsonObject["type"];
	if (!type.isString())
		return false;

	const auto invoker = FindSecond(PARSERS, type.toString().toStdString().data(), &stub, PszComparer {});
	const auto result = invoker(jsonObject);
	if (result.isEmpty())
		return false;

	client->OnTextReceived(result);
	return true;
}
