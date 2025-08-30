#include "AuthorReviewModel.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/AuthorReviewModelRole.h"

#include "inpx/src/util/constant.h"
#include "util/localization.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Role = AuthorReviewModelRole;

class Model final : public QAbstractListModel
{
	struct Item
	{
		long long id;
		QString time;
		QString name;
		QString title;
		QString text;
	};

	using Items = std::vector<Item>;

public:
	static std::unique_ptr<QAbstractItemModel> Create(const ISettings& settings, const ICollectionProvider& collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser)
	{
		return std::make_unique<Model>(settings.Get(Constant::Settings::SHOW_REMOVED_BOOKS_KEY, false),
		                               collectionProvider.ActiveCollectionExists() ? collectionProvider.GetActiveCollection().folder : QString {},
		                               std::move(databaseUser));
	}

	Model(const bool showRemoved, const QString& folder, std::shared_ptr<const IDatabaseUser> databaseUser)
		: m_showRemoved { showRemoved }
		, m_folder { folder + "/" + QString::fromStdWString(REVIEWS_FOLDER) }
		, m_databaseUser { std::move(databaseUser) }
	{
	}

private: // QAbstractItemModel
	int rowCount(const QModelIndex& parent) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(std::size(m_items));
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		assert(index.isValid() && index.row() < rowCount({}));
		const auto& item = m_items[index.row()];

		switch (role)
		{
			case Qt::DisplayRole:
				return QString("%1, %2. \"%3\"\n%4").arg(item.time, item.name, item.title, item.text);

			case Role::BookId:
				return item.id;

			default:
				break;
		}
		return {};
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		if (role != Role::AuthorId)
			return QAbstractListModel::setData(index, value, role);

		Reset(value.toLongLong());
		return true;
	}

private:
	void Reset(const long long authorId)
	{
		auto db = m_databaseUser->Database();
		m_databaseUser->Execute({ "Get author reviews",
		                          [this, authorId, db = std::move(db)]
		                          {
									  return [this, items = GetReviews(authorId, *db)](size_t) mutable
									  {
										  ScopedCall resetGuard([this] { beginResetModel(); }, [this] { endResetModel(); });
										  m_items = std::move(items);
									  };
								  } });
	}

	Items GetReviews(const long long authorId, DB::IDatabase& db) const
	{
		const auto query = db.CreateQuery(R"(
select r.Folder, b.BookID, b.LibID, b.Title 
	from Reviews r 
	join Books_View b on b.BookID = r.BookID and b.IsDeleted != ? 
	join Author_List a on a.BookID = r.BookID and a.AuthorID = ? 
	order by r.Folder
)");
		query->Bind(0, m_showRemoved ? 2 : 1);
		query->Bind(1, authorId);

		Items items;

		std::unique_ptr<Zip> zip;

		QString folder;
		for (query->Execute(); !query->Eof(); query->Next())
		{
			if (folder != query->Get<const char*>(0))
			{
				folder = query->Get<const char*>(0);
				try
				{
					zip = std::make_unique<Zip>(m_folder.filePath(folder));
				}
				catch (const std::exception& ex)
				{
					PLOGE << ex.what();
					continue;
				}
				catch (...)
				{
					PLOGE << "unknown error";
					continue;
				}
			}

			const auto bookId = query->Get<long long>(1);
			const QString libId = query->Get<const char*>(2);
			const QString title = query->Get<const char*>(3);

			QJsonParseError parseError;
			const auto doc = QJsonDocument::fromJson(zip->Read(libId)->GetStream().readAll(), &parseError);
			if (parseError.error != QJsonParseError::NoError)
			{
				PLOGE << parseError.errorString();
				continue;
			}

			assert(doc.isArray());
			for (const auto reviewValue : doc.array())
			{
				assert(reviewValue.isObject());
				const auto reviewObj = reviewValue.toObject();
				auto& item = items.emplace_back(bookId, reviewObj[Constant::TIME].toString(), reviewObj[Constant::NAME].toString(), title, reviewObj[Constant::TEXT].toString());
				item.text.replace("<br/>", "\n").append('\n');
				if (item.name.isEmpty())
					item.name = Loc::Tr(Loc::Ctx::COMMON, Loc::ANONYMOUS);
			}
		}

		return items;
	}

private:
	const bool m_showRemoved;
	const QDir m_folder;
	const std::shared_ptr<const IDatabaseUser> m_databaseUser;
	Items m_items;
};

} // namespace

AuthorReviewModel::AuthorReviewModel(const std::shared_ptr<const ISettings>& settings,
                                     const std::shared_ptr<const ICollectionProvider>& collectionProvider,
                                     std::shared_ptr<const IDatabaseUser> databaseUser,
                                     QObject* parent)
	: QSortFilterProxyModel(parent)
	, m_source { Model::Create(*settings, *collectionProvider, std::move(databaseUser)) }
{
	QSortFilterProxyModel::setSourceModel(m_source.get());

	PLOGV << "AuthorReviewModel created";
}

AuthorReviewModel::~AuthorReviewModel()
{
	PLOGV << "AuthorReviewModel destroyed";
}
