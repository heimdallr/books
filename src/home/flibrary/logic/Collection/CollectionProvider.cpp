#include "CollectionProvider.h"

#include <QDir>
#include <QTimer>

#include "fnd/ScopedCall.h"
#include "fnd/memory.h"
#include "fnd/observable.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/localization.h"
#include "interface/logic/IDatabaseUser.h"

#include "platform/StrUtil.h"
#include "util/IExecutor.h"
#include "util/files.h"
#include "util/hash.h"

#include "CollectionImpl.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

auto GetInpxImpl(const QString& folder)
{
	std::set<QString> result;
	std::ranges::transform(QDir(folder).entryList({ "*.inpx" }), std::inserter(result, result.end()), [&](const auto& item) {
		return Util::ToAbsolutePath(QString("%1/%2").arg(folder, item));
	});
	return result;
}

constexpr auto CONTEXT    = "CollectionStatistics";
constexpr auto STATISTICS = QT_TRANSLATE_NOOP("CollectionStatistics", "Collection statistics:");
constexpr auto NAME       = QT_TRANSLATE_NOOP("CollectionStatistics", "Name: %1");
constexpr auto FOLDER     = QT_TRANSLATE_NOOP("CollectionStatistics", "Folder: %1");
constexpr auto ADDITIONAL = QT_TRANSLATE_NOOP("CollectionStatistics", "Additional info: %1");
constexpr auto INPX       = QT_TRANSLATE_NOOP("CollectionStatistics", "Index file: %1");
constexpr auto DATABASE   = QT_TRANSLATE_NOOP("CollectionStatistics", "Database: %1");

TR_DEF

}

class CollectionProvider::Impl final : public Observable<ICollectionsObserver>
{
public:
	explicit Impl(std::shared_ptr<ISettings> settings)
		: m_settings { std::move(settings) }
	{
	}

	Collections& GetCollections() noexcept
	{
		return m_collections;
	}

	Collection& GetActiveCollection() noexcept
	{
		const auto id         = GetActiveCollectionId();
		auto       collection = FindCollectionById(id);
		assert(collection);
		return *collection;
	}

	bool ActiveCollectionExists() noexcept
	{
		const auto  id         = GetActiveCollectionId();
		const auto* collection = FindCollectionById(id);
		return !!collection;
	}

	Collection* FindCollectionById(const QString& id) noexcept
	{
		const auto it = std::ranges::find(m_collections, id, [&](const auto& item) {
			return item->id;
		});
		if (it == std::cend(m_collections))
			return nullptr;

		auto& collection = **it;
		return &collection;
	}

	QString GetActiveCollectionId() const noexcept
	{
		return CollectionImpl::GetActive(*m_settings);
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	Collections                                   m_collections { CollectionImpl::Deserialize(*m_settings) };
};

CollectionProvider::CollectionProvider(std::shared_ptr<ISettings> settings)
	: m_impl { std::make_unique<Impl>(std::move(settings)) }
{
	PLOGV << "CollectionProvider created";
}

CollectionProvider::~CollectionProvider()
{
	PLOGV << "CollectionProvider destroyed";
}

bool CollectionProvider::IsEmpty() const noexcept
{
	return GetCollections().empty();
}

bool CollectionProvider::IsCollectionNameExists(const QString& name) const
{
	return std::ranges::any_of(GetCollections(), [&](const auto& item) {
		return item->name == name;
	});
}

QString CollectionProvider::GetCollectionDatabaseName(const QString& databaseFileName) const
{
	const auto collection = m_impl->FindCollectionById(Util::md5(databaseFileName.toUtf8()));
	return collection ? collection->name : QString {};
}

std::set<QString> CollectionProvider::GetInpxFiles(const QString& folder) const
{
	return GetInpxImpl(folder);
}

bool CollectionProvider::IsCollectionFolderHasInpx(const QString& folder) const
{
	return !GetInpxFiles(folder).empty();
}

Collections& CollectionProvider::GetCollections() noexcept
{
	return m_impl->GetCollections();
}

const Collections& CollectionProvider::GetCollections() const noexcept
{
	return m_impl->GetCollections();
}

Collection& CollectionProvider::GetActiveCollection() noexcept
{
	return m_impl->GetActiveCollection();
}

const Collection& CollectionProvider::GetActiveCollection() const noexcept
{
	return m_impl->GetActiveCollection();
}

bool CollectionProvider::ActiveCollectionExists() const noexcept
{
	return m_impl->ActiveCollectionExists();
}

QString CollectionProvider::GetActiveCollectionId() const noexcept
{
	return m_impl->GetActiveCollectionId();
}

QStringList CollectionProvider::GetCollectionStatistics(const IDatabaseUser& databaseUser, const bool withAdditionalInfo) const
{
	static constexpr auto dbStatQueryText = "select '%1', count(42) from Folders union all "
											"select '%2', count(42) from Authors union all "
											"select '%3', count(42) from Series union all "
											"select '%4', count(42) from Keywords union all "
											"select '%5', count(42) from Languages l where (select count(42) from Books b where b.Lang = l.LanguageCode) > 1 union all "
											"select '%6', count(42) from Groups_User union all "
											"select '%7', count(42) from Books union all "
											"select '%8', count(42) from Books b left join Books_User bu on bu.BookID = b.BookID where coalesce(bu.IsDeleted, b.IsDeleted, 0) != 0";

	QStringList stats;
	if (withAdditionalInfo)
	{
		const auto& collection = GetActiveCollection();
		stats << Tr(STATISTICS) << Tr(NAME).arg(collection.name) << Tr(FOLDER).arg(QDir::toNativeSeparators(collection.GetFolder()));
		if (const auto value = collection.GetAdditionalFolder(); !value.isEmpty())
			stats << Tr(ADDITIONAL).arg(QDir::toNativeSeparators(value));
		if (const auto value = collection.GetInpx(); !value.isEmpty())
			stats << Tr(INPX).arg(QDir::toNativeSeparators(value));
		stats << Tr(DATABASE).arg(QDir::toNativeSeparators(collection.GetDatabase()));
	}

	const auto bookQuery = databaseUser.Database()->CreateQuery(QString(dbStatQueryText)
	                                                                .arg(
																		QT_TRANSLATE_NOOP("CollectionStatistics", "Archives:"),
																		QT_TRANSLATE_NOOP("CollectionStatistics", "Authors:"),
																		QT_TRANSLATE_NOOP("CollectionStatistics", "Series:"),
																		QT_TRANSLATE_NOOP("CollectionStatistics", "Keywords:"),
																		QT_TRANSLATE_NOOP("CollectionStatistics", "Languages:"),
																		QT_TRANSLATE_NOOP("CollectionStatistics", "Groups:"),
																		QT_TRANSLATE_NOOP("CollectionStatistics", "Books:"),
																		QT_TRANSLATE_NOOP("CollectionStatistics", "Deleted books:")
																	)
	                                                                .toStdString());
	for (bookQuery->Execute(); !bookQuery->Eof(); bookQuery->Next())
	{
		[[maybe_unused]] const auto* name       = bookQuery->Get<const char*>(0);
		[[maybe_unused]] const auto  translated = Loc::Tr(CONTEXT, bookQuery->Get<const char*>(0));
		stats << QString("%1 %2").arg(translated).arg(bookQuery->Get<long long>(1));
	}

	return stats;
}

void CollectionProvider::RegisterObserver(ICollectionsObserver* observer)
{
	m_impl->Register(observer);
}

void CollectionProvider::UnregisterObserver(ICollectionsObserver* observer)
{
	m_impl->Unregister(observer);
}

void CollectionProvider::OnActiveCollectionChanged()
{
	m_impl->Perform(&ICollectionsObserver::OnActiveCollectionChanged);
}

void CollectionProvider::OnNewCollectionCreating(const bool value)
{
	m_impl->Perform(&ICollectionsObserver::OnNewCollectionCreating, value);
}
