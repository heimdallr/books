#include <QString>

#include "fnd/FindPair.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"

#include "constants/books.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData { namespace {

struct Book
{
	QString folder;
	QString fileName;

	explicit Book(const Util::XmlAttributes& attributes)
		: folder { attributes.GetAttribute(Constant::UserData::Books::Folder).toString() }
		, fileName { attributes.GetAttribute(Constant::UserData::Books::FileName).toString() }
	{
	}
};

class BooksRestorer final : public IRestorer
{
	struct Item : Book
	{
		QString isDeleted;
		QString userRate;

		explicit Item(const Util::XmlAttributes& attributes)
			: Book(attributes)
			, isDeleted { attributes.GetAttribute(Constant::UserData::Books::IsDeleted).toString() }
			, userRate { attributes.GetAttribute(Constant::UserData::Books::UserRate).toString() }
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
		static constexpr auto commandText = "insert into Books_User(BookID, IsDeleted, UserRate) "
											"select BookID, ?, ? "
											"from Books "
											"where FolderID = (select FolderID from Folders where FolderTitle = ?) and FileName = ?";

		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Books_User")->Execute();

		const auto command = transaction->CreateCommand(commandText);

		const auto getInt = [&](const QString& value) -> std::optional<int> {
			bool ok = false;
			const auto result = value.toInt(&ok);
			return ok ? std::optional { result } : std::nullopt;
		};

		for (const auto& item : m_items)
			command->Execute(getInt(item.isDeleted), getInt(item.userRate), item.folder, item.fileName);

		transaction->Commit();
	}

private:
	Items m_items;
};

}} // namespace HomeCompa::Flibrary::UserData

namespace HomeCompa::Flibrary::UserData {

std::unique_ptr<IRestorer> CreateBooksRestorer2()
{
	return std::make_unique<BooksRestorer>();
}

} // namespace HomeCompa::Flibrary::UserData
