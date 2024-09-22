#include "fnd/FindPair.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"

#include "constants/books.h"

#include "database/DatabaseUser.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData {

namespace {

struct Book
{
	QString folder;
	QString fileName;

	explicit Book(const Util::XmlAttributes & attributes)
		: folder(attributes.GetAttribute(Constant::UserData::Books::Folder))
		, fileName(attributes.GetAttribute(Constant::UserData::Books::FileName))
	{
	}
};

class BooksRestorer final
	: virtual public IRestorer
{
	struct Item : Book
	{
		QString isDeleted;
		QString userRate;

		explicit Item(const Util::XmlAttributes & attributes)
			: Book(attributes)
			, isDeleted(attributes.GetAttribute(Constant::UserData::Books::IsDeleted))
			, userRate(attributes.GetAttribute(Constant::UserData::Books::UserRate))
		{
		}
	};
	using Items = std::vector<Item>;

private: // IRestorer
	void AddElement([[maybe_unused]] const QString & name, const Util::XmlAttributes & attributes) override
	{
		assert(name == Constant::ITEM);
		using namespace Constant::UserData::Books;
		m_items.emplace_back(attributes);
	}

	void Restore(DB::IDatabase & db) const override
	{
		using namespace Constant::UserData::Books;
		static constexpr auto commandText =
			"insert into Books_User(BookID, IsDeleted, UserRate) "
			"select BookID, ?, ? "
			"from Books "
			"where Folder = ? and FileName = ?"
			;

		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Books_User")->Execute();

		const auto command = transaction->CreateCommand(commandText);

		const auto bind = [&] (const size_t index, const QString& value)
		{
			bool ok = false;
			if (const auto userRate = value.toInt(&ok); ok)
				command->Bind(index, userRate);
			else
				command->Bind(index);
		};

		for (const auto & item : m_items)
		{
			bind(0, item.isDeleted);
			bind(1, item.userRate);
			command->Bind(2, item.folder.toStdString());
			command->Bind(3, item.fileName.toStdString());
			command->Execute();
		}

		transaction->Commit();
	}

private:
	Items m_items;
};

}

}

namespace HomeCompa::Flibrary::UserData {

std::unique_ptr<IRestorer> CreateBooksRestorer2()
{
	return std::make_unique<BooksRestorer>();
}

}
