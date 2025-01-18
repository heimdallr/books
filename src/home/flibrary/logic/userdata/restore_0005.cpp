#include <QString>

#include "fnd/FindPair.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"

#include "constants/books.h"
#include "constants/searches.h"

#include "ChangeNavigationController/SearchController.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData {

namespace {

struct Created
{
	QString title;
	int mode;
	QString createdAt;
};

void Bind(DB::ICommand & command, const size_t index, const QString & value)
{
	if (value.isEmpty())
		command.Bind(index);
	else
		command.Bind(index, value.toStdString());
};

QString CreateSearchTitle(QString title, int mode)
{
	mode = ~mode;

	if (mode & SearchController::SearchMode::EndsWith && title.endsWith('~'))
		title.resize(title.length() - 1);
	if (mode & SearchController::SearchMode::StartsWith && title.startsWith('~'))
		title.remove(0, 1);

	return title;
}

class SearchesRestorer final
	: virtual public IRestorer
{
private: // IRestorer
	void AddElement([[maybe_unused]] const QString & name, const Util::XmlAttributes & attributes) override
	{
		assert(name == Constant::ITEM);
		auto & item = m_items.emplace_back();
		item.title = attributes.GetAttribute(Constant::TITLE);
		item.mode = attributes.GetAttribute(Constant::UserData::Searches::Mode).toInt();
		item.createdAt = attributes.GetAttribute(Constant::UserData::Books::CreatedAt);
	}

	void Restore(DB::IDatabase & db) const override
	{
		using namespace Constant::UserData;
		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Searches_User")->Execute();
		const auto createSearchCommand = transaction->CreateCommand(Searches::CreateNewSearchCommandText);
		for (const auto & [title, mode, createdAt] : m_items)
		{
			createSearchCommand->Bind(0, title.toStdString());
			createSearchCommand->Bind(1, mode);
			createSearchCommand->Bind(2, CreateSearchTitle(title, mode).toUpper().toStdString());
			Bind(*createSearchCommand, 3, createdAt);
			createSearchCommand->Execute();
		}

		transaction->Commit();
	}

private:
	std::vector<Created> m_items;
};

}

}

namespace HomeCompa::Flibrary::UserData {

std::unique_ptr<IRestorer> CreateSearchesRestorer5()
{
	return std::make_unique<SearchesRestorer>();
}

}
