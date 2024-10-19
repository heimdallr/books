#include "fnd/FindPair.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"

#include "constants/books.h"
#include "constants/ExportStat.h"

#include "database/DatabaseUser.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData {

namespace {

struct Book
{
	QString folder;
	QString fileName;
	QString exportType;
	QString createdAt;

	explicit Book(const Util::XmlAttributes & attributes)
		: folder(attributes.GetAttribute(Constant::UserData::Books::Folder))
		, fileName(attributes.GetAttribute(Constant::UserData::Books::FileName))
		, exportType(attributes.GetAttribute(Constant::UserData::ExportStat::ExportType))
		, createdAt(attributes.GetAttribute(Constant::UserData::Books::CreatedAt))
	{
	}
};
using Books = std::vector<Book>;

class ExportStatRestorer final
	: virtual public IRestorer
{
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
			"insert into Export_List_User(BookID, ExportType, CreatedAt) "
			"select BookID, ?, ? "
			"from Books "
			"where Folder = ? and FileName = ?"
			;

		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Export_List_User")->Execute();

		const auto command = transaction->CreateCommand(commandText);

		for (const auto & item : m_items)
		{
			command->Bind(0, item.exportType.toInt());
			command->Bind(1, item.createdAt.toStdString());
			command->Bind(2, item.folder.toStdString());
			command->Bind(3, item.fileName.toStdString());
			command->Execute();
		}

		transaction->Commit();
	}

private:
	Books m_items;
};

}

std::unique_ptr<IRestorer> CreateExportStatRestorer4()
{
	return std::make_unique<ExportStatRestorer>();
}

}
