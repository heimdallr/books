#include <QString>

#include "fnd/FindPair.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IDatabaseUser.h"

#include "constants/books.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData
{

namespace
{

struct Book
{
	QString folder;
	QString fileName;
	QString createdAt;

	explicit Book(const Util::XmlAttributes& attributes)
		: folder(attributes.GetAttribute(Constant::UserData::Books::Folder))
		, fileName(attributes.GetAttribute(Constant::UserData::Books::FileName))
		, createdAt(attributes.GetAttribute(Constant::UserData::Books::CreatedAt))
	{
	}
};

#define ADDITIONAL_BOOK_FIELDS_X_MACRO \
	ADDITIONAL_BOOK_FIELD(IsDeleted)   \
	ADDITIONAL_BOOK_FIELD(UserRate)    \
	ADDITIONAL_BOOK_FIELD(Lang)        \
	ADDITIONAL_BOOK_FIELD(CreatedAt)

struct FieldNo
{
	enum
	{

#define ADDITIONAL_BOOK_FIELD(NAME) NAME,
		ADDITIONAL_BOOK_FIELDS_X_MACRO
#undef ADDITIONAL_BOOK_FIELD
			Folder,
		FileName,
	};
};

class BooksRestorer final : virtual public IRestorer
{
	struct Item : Book
	{
#define ADDITIONAL_BOOK_FIELD(NAME) QString NAME;
		ADDITIONAL_BOOK_FIELDS_X_MACRO
#undef ADDITIONAL_BOOK_FIELD

		explicit Item(const Util::XmlAttributes& attributes)
			: Book(attributes)
#define ADDITIONAL_BOOK_FIELD(NAME) , NAME(attributes.GetAttribute(Constant::UserData::Books::NAME))
				  ADDITIONAL_BOOK_FIELDS_X_MACRO
#undef ADDITIONAL_BOOK_FIELD
		{
		}
	};

	using Items = std::vector<Item>;

private: // IRestorer
	void AddElement([[maybe_unused]] const QString& name, const Util::XmlAttributes& attributes) override
	{
		assert(name == Constant::ITEM);
		using namespace Constant::UserData::Books;
		m_items.emplace_back(attributes);
	}

	void Restore(DB::IDatabase& db) const override
	{
		using namespace Constant::UserData::Books;

		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Books_User")->Execute();

		{
			static constexpr auto commandText = "insert into Books_User(BookID, IsDeleted, UserRate, Lang, CreatedAt) "
												"select BookID, ?, ?, ?, ? "
												"from Books "
												"where FolderID = (select FolderID from Folders where FolderTitle = ?) and FileName = ?";
			const auto            command     = transaction->CreateCommand(commandText);

			const auto bind = [&](const size_t index, const QString& value) {
				if (value.isEmpty())
					return (void)command->Bind(index);

				bool ok = false;
				if (const auto integer = value.toInt(&ok); ok)
					command->Bind(index, integer);
				else
					command->Bind(index, value.toStdString());
			};

			for (const auto& item : m_items)
			{
#define ADDITIONAL_BOOK_FIELD(NAME) bind(FieldNo::NAME, item.NAME);
				ADDITIONAL_BOOK_FIELDS_X_MACRO
#undef ADDITIONAL_BOOK_FIELD

				command->Bind(FieldNo::Folder, item.folder.toStdString());
				command->Bind(FieldNo::FileName, item.fileName.toStdString());
				command->Execute();
			}
		}
		{
			static constexpr auto commandText = "update Books set Lang = (select Lang from Books_User where Books_User.BookID = Books.BookID) "
												"where exists(select 42 from Books_User where Books_User.BookID = Books.BookID and Books_User.Lang is not null)";
			transaction->CreateCommand(commandText)->Execute();
		}

		transaction->Commit();
	}

private:
	Items m_items;
};

#undef ADDITIONAL_BOOK_FIELDS_X_MACRO

} // namespace

} // namespace HomeCompa::Flibrary::UserData

namespace HomeCompa::Flibrary::UserData
{

std::unique_ptr<IRestorer> CreateBooksRestorer6()
{
	return std::make_unique<BooksRestorer>();
}

}
