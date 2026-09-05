#include <zlib.h>

#include <array>

#include <QFile>

#include "fnd/NonCopyMovable.h"

#include "database/interface/IDatabase.h"
#include "database/interface/ITemporaryTable.h"
#include "database/interface/ITransaction.h"

#include "interface/logic/IDatabaseUser.h"

#include "constants/UserData.h"
#include "constants/groups.h"
#include "util/IExecutor.h"
#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"

#include "log.h"

namespace HomeCompa::Flibrary::UserData
{

namespace
{

class ZDecompressionStream final : public QIODevice
{
	NON_COPY_MOVABLE(ZDecompressionStream)

	static constexpr size_t BUFFER_SIZE = 4096;

public:
	explicit ZDecompressionStream(QIODevice& source)
		: m_sourceStream { source }
	{
		if (inflateInit2(&m_zst, 15 + 32) != Z_OK)
			throw std::runtime_error("Cannot initialize zlib inflate.");
	}

	~ZDecompressionStream() override
	{
		inflateEnd(&m_zst);
	}

private: // QIODevice
	qint64 readData(char* destBuffer, const qint64 bytesToRead) override
	{
		if (bytesToRead <= 0 || m_isFinished)
			return 0;

		m_zst.next_out  = reinterpret_cast<Bytef*>(destBuffer);
		m_zst.avail_out = static_cast<uInt>(bytesToRead);

		while (m_zst.avail_out > 0)
		{
			if (m_zst.avail_in == 0 && !m_sourceStream.atEnd())
			{
				const auto count = m_sourceStream.read(m_inBuffer.data(), BUFFER_SIZE);
				m_zst.avail_in   = static_cast<uInt>(count);
				m_zst.next_in    = reinterpret_cast<Bytef*>(m_inBuffer.data());
			}

			if (m_zst.avail_in == 0 && !m_sourceStream.atEnd())
				break;

			int ret = inflate(&m_zst, Z_NO_FLUSH);

			if (ret == Z_STREAM_END)
			{
				m_isFinished = true;
				break;
			}

			if (ret != Z_OK)
				throw std::runtime_error(std::format("zlib decode error: {}. {}", ret, m_zst.msg));
		}

		return bytesToRead - m_zst.avail_out;
	}

	qint64 writeData(const char* /*data*/, qint64 /*len*/) override
	{
		return assert(false && "no impl"), 0;
	}

private:
	QIODevice& m_sourceStream;
	z_stream   m_zst {
		.next_in   = nullptr,
		.avail_in  = 0,
		.total_in  = 0,
		.next_out  = nullptr,
		.avail_out = 0,
		.total_out = 0,
		.msg       = nullptr,
		.state     = nullptr,
		.zalloc    = nullptr,
		.zfree     = nullptr,
		.opaque    = nullptr,
		.data_type = -1,
		.adler     = 0,
		.reserved  = 0,
	};
	bool m_isFinished { false };

	std::array<char, BUFFER_SIZE> m_inBuffer {};
};

class XmlParser final : public Util::SaxParser
{
	struct Extra
	{
		QString libId;
		int     rate { -1 };
	};

	using Extras = std::vector<Extra>;

	struct Group
	{
		QString                     name;
		long long                   id { -1 };
		std::unordered_set<QString> libIds;
	};

	using Groups = std::vector<Group>;

public:
	explicit XmlParser(QIODevice& stream, DB::IDatabase& db)
		: SaxParser(stream)
	{
		Parse();
		Write(db);
	}

private: // Util::SaxParser
	bool OnStartElement(QStringView /*name*/, const QStringView path, const Util::XmlAttributes& attributes) override
	{
		if (path == "UserData/Extras/Book")
			return m_extras.emplace_back(attributes.GetAttribute(L"libid").toString(), attributes.GetAttribute(L"rate").toInt()), true;

		if (path == "UserData/Groups/Group")
			return m_groups.emplace_back(attributes.GetAttribute(L"name").toString()), true;

		if (path == "UserData/Groups/Group/Book")
			return assert(!m_groups.empty()), m_groups.back().libIds.emplace(attributes.GetAttribute(L"libid")), true;

		return true;
	}

private:
	void Write(DB::IDatabase& db)
	{
		const auto tmpBooksUser      = db.CreateTemporaryTable({ "LibID VARCHAR (200)", "UserRate  INTEGER" });
		const auto tmpGroupsListUser = db.CreateTemporaryTable({ "LibID VARCHAR (200)", "GroupID  INTEGER" });

		const auto tr = db.CreateTransaction();
		{
			tr->CreateCommand("delete from Books_User")->Execute();

			const auto command = tr->CreateCommand(std::format("insert into {}(LibID, UserRate) values(?, ?)", tmpBooksUser->GetTableName()));
			for (const auto& [id, rate] : m_extras)
			{
				command->Bind(0, id);
				command->Bind(1, rate);
				command->Execute();
			}
			tr->CreateCommand(
				  std::format(
					  "insert into Books_User(BookID, UserRate, CreatedAt) select b.BookID, t.UserRate, datetime('now', 'localtime') from Books b join {} t on t.LibID = b.LibID",
					  tmpBooksUser->GetTableName()
				  )
			)
				->Execute();
		}
		{
			tr->CreateCommand("delete from Groups_User")->Execute();

			{
				const auto createGroupCommand = tr->CreateCommand(Constant::UserData::Groups::CreateNewGroupCommandText);
				for (auto& group : m_groups)
				{
					createGroupCommand->Bind(0, group.name);
					createGroupCommand->Execute();

					const auto getLastIdQuery = tr->CreateQuery(IDatabaseUser::SELECT_LAST_ID_QUERY);
					getLastIdQuery->Execute();
					group.id = getLastIdQuery->Get<long long>(0);
				}
			}
			const auto command = tr->CreateCommand(std::format("insert into {}(LibID, GroupID) values(?, ?)", tmpGroupsListUser->GetTableName()));
			for (const auto& group : m_groups)
				for (const auto& libId : group.libIds)
				{
					command->Bind(0, libId);
					command->Bind(1, group.id);
					command->Execute();
				}
			tr->CreateCommand(
				  std::format(
					  "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) select t.GroupID, b.BookID, datetime('now', 'localtime') from Books b join {} t on t.LibID = b.LibID",
					  tmpGroupsListUser->GetTableName()
				  )
			)
				->Execute();
		}
		tr->Commit();
	}

private:
	Extras m_extras;
	Groups m_groups;
};

} // namespace

void ImportFromMyHomeLib(const Util::IExecutor& executor, DB::IDatabase& db, QString fileName, Callback callback)
{
	executor({ "Restore MHL user data", [&db, fileName = std::move(fileName), callback = std::move(callback)]() mutable {
				  auto createResult = [callback = std::move(callback)](QString error = {}) mutable {
					  if (!error.isEmpty())
					  {
						  PLOGE << error;
					  }

					  return [callback = std::move(callback), error = std::move(error)](size_t) {
						  callback(error);
					  };
				  };

				  QFile inp(fileName);
				  if (!inp.open(QIODevice::ReadOnly))
					  return createResult(QString(CANNOT_READ_FROM).arg(fileName));

				  try
				  {
					  ZDecompressionStream stream(inp);
					  stream.open(QIODevice::ReadOnly);
					  const XmlParser parser(stream, db);
				  }
				  catch (const std::exception& ex)
				  {
					  PLOGE << ex.what();
					  return createResult(ex.what());
				  }
				  return createResult();
			  } });
}

} // namespace HomeCompa::Flibrary::UserData
