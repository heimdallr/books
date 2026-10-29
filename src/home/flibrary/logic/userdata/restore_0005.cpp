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
	QString title;
	QString createdAt;
};

std::optional<QString> GetStringParameter(QString value)
{
	return !value.isEmpty() ? std::optional { std::move(value) } : std::nullopt;
}

class SearchesRestorer final : public IRestorer
{
private: // IRestorer
	void AddElement([[maybe_unused]] const QString& name, const Util::XmlAttributes& attributes) override
	{
		assert(name == Constant::ITEM);
		auto& item     = m_items.emplace_back();
		item.title     = attributes.GetAttribute(Constant::TITLE).toString();
		item.createdAt = attributes.GetAttribute(Constant::UserData::Books::CreatedAt).toString();
	}

	void Restore(DB::IDatabase& db) const override
	{
		using namespace Constant::UserData;
		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Searches_User")->Execute();
		for (const auto createSearchCommand = transaction->CreateCommand(Searches::CreateNewSearchCommandText); const auto& [title, createdAt] : m_items)
			createSearchCommand->Execute(title, GetStringParameter(createdAt));

		transaction->Commit();
	}

private:
	std::vector<Created> m_items;
};

}} // namespace HomeCompa::Flibrary::UserData

namespace HomeCompa::Flibrary::UserData {

std::unique_ptr<IRestorer> CreateSearchesRestorer5()
{
	return std::make_unique<SearchesRestorer>();
}

} // namespace HomeCompa::Flibrary::UserData
