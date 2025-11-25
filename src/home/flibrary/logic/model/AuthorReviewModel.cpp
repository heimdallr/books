#include "AuthorReviewModel.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"
#include "interface/logic/AuthorReviewModelRole.h"

#include "Constant.h"
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
		QString   time;
		QString   name;
		QString   title;
		QString   text;
	};

	using Items = std::vector<Item>;

public:
	static std::unique_ptr<QAbstractItemModel> Create(const ISettings& settings, const ICollectionProvider& collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser)
	{
		return std::make_unique<Model>(
			settings.Get(Constant::Settings::SHOW_REMOVED_BOOKS_KEY, false),
			collectionProvider.ActiveCollectionExists() ? collectionProvider.GetActiveCollection().GetFolder() : QString {},
			std::move(databaseUser)
		);
	}

	Model(const bool showRemoved, const QString& folder, std::shared_ptr<const IDatabaseUser> databaseUser)
		: m_showRemoved { showRemoved }
		, m_folder { folder + "/" + QString::fromStdWString(Inpx::REVIEWS_FOLDER) }
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
		m_databaseUser->Execute({ "Get author reviews", [this, authorId, db = std::move(db)] {
									 return [this, items = GetReviews(authorId, *db)](size_t) mutable {
										 ScopedCall resetGuard(
											 [this] {
												 beginResetModel();
											 },
											 [this] {
												 endResetModel();
											 }
										 );
										 m_items = std::move(items);
									 };
								 } });
	}

	Items GetReviews(const long long authorId, DB::IDatabase& db) const
	{
		const auto query = db.CreateQuery(R"(
select coalesce(b.SourceLib || '/', '') || r.Folder, b.BookID, b.LibID, b.Title 
	from Reviews r 
	join Books_View b on b.BookID = r.BookID and b.IsDeleted != ? 
	join Author_List a on a.BookID = r.BookID and a.AuthorID = ? 
	order by r.Folder
)");
		query->Bind(0, m_showRemoved ? 2 : 1);
		query->Bind(1, authorId);

		Items items;

		std::shared_ptr<const Zip> zip;
		QString                    folder;
		const auto                 getZip = [&] {
            if (!Util::Set(folder, QString(query->Get<const char*>(0))))
                return assert(zip);

            try
            {
                zip.reset();
                zip = std::make_shared<Zip>(m_folder.filePath(folder));
            }
            catch (const std::exception& ex)
            {
                PLOGE << ex.what();
            }
            catch (...)
            {
                PLOGE << "unknown error";
            }
		};

		for (query->Execute(); !query->Eof(); query->Next())
			if (getZip(), zip)
				GetReviews(items, *query, *zip);

		return items;
	}

	void GetReviews(Items& items, const DB::IQuery& query, const Zip& zip) const
	{
		const auto    bookId = query.Get<long long>(1);
		const QString libId  = query.Get<const char*>(2);
		QString       title  = query.Get<const char*>(3);

		const auto stream = zip.Read(libId);
		if (!stream)
		{
			PLOGE << "Cannot extract " << libId;
			return;
		}

		QJsonParseError parseError;
		const auto      doc = QJsonDocument::fromJson(stream->GetStream().readAll(), &parseError);
		if (parseError.error != QJsonParseError::NoError)
		{
			PLOGE << parseError.errorString();
			return;
		}

		auto toItem = [bookId, title = std::move(title)](auto&& reviewValue) {
			assert(reviewValue.isObject());
			const auto reviewObject = reviewValue.toObject();
			auto       name         = reviewObject[Inpx::NAME].toString();
			return Item { bookId,
				          reviewObject[Inpx::TIME].toString(),
				          name.isEmpty() ? Loc::Tr(Loc::Ctx::COMMON, Loc::ANONYMOUS) : std::move(name),
				          title,
				          reviewObject[Inpx::TEXT].toString().replace("<br/>", "\n").append('\n') };
		};

		assert(doc.isArray());
		std::ranges::transform(doc.array(), std::back_inserter(items), std::move(toItem));
	}

private:
	const bool                                 m_showRemoved;
	const QDir                                 m_folder;
	const std::shared_ptr<const IDatabaseUser> m_databaseUser;
	Items                                      m_items;
};

} // namespace

AuthorReviewModel::AuthorReviewModel(
	const std::shared_ptr<const ISettings>&           settings,
	const std::shared_ptr<const ICollectionProvider>& collectionProvider,
	std::shared_ptr<const IDatabaseUser>              databaseUser,
	QObject*                                          parent
)
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
