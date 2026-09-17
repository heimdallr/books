#include <QString>

#include "fnd/FindPair.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IBookSearchController.h"

#include "constants/books.h"
#include "constants/searches.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData { namespace {

struct Created
{
	QString origin;
	QString title;
	QString createdAt;
};

void Bind(DB::ICommand& command, const size_t index, const QString& value)
{
	if (value.isEmpty())
		command.Bind(index);
	else
		command.Bind(index, value.toStdString());
}

class SearchesRestorer final : virtual public IRestorer
{
private: // IRestorer
	void AddElement([[maybe_unused]] const QString& name, const Util::XmlAttributes& attributes) override
	{
		assert(name == Constant::ITEM);
		auto& item     = m_items.emplace_back();
		item.origin    = attributes.GetAttribute(Constant::UserData::Searches::Origin).toString();
		item.title     = attributes.GetAttribute(Constant::TITLE).toString();
		item.createdAt = attributes.GetAttribute(Constant::UserData::Books::CreatedAt).toString();
	}

	void Restore(DB::IDatabase& db) const override
	{
		using namespace Constant::UserData;
		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Searches_User")->Execute();
		const auto createSearchCommand = transaction->CreateCommand(Searches::CreateNewSearchCommandText);
		for (const auto& [origin, title, createdAt] : m_items)
		{
			assert(!title.isEmpty());
			Bind(*createSearchCommand, 0, origin);
			createSearchCommand->Bind(1, title);
			Bind(*createSearchCommand, 2, createdAt);
			createSearchCommand->Execute();
		}

		transaction->Commit();
	}

private:
	std::vector<Created> m_items;
};

}} // namespace HomeCompa::Flibrary::UserData

namespace HomeCompa::Flibrary::UserData {

std::unique_ptr<IRestorer> CreateSearchesRestorer9()
{
	return std::make_unique<SearchesRestorer>();
}

} // namespace HomeCompa::Flibrary::UserData
