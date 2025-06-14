#include "CollectionProvider.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimer>

#include "fnd/ScopedCall.h"
#include "fnd/memory.h"
#include "fnd/observable.h"

#include "inpx/src/util/inpx.h"
#include "util/IExecutor.h"
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
	std::ranges::transform(QDir(folder).entryList({ "*.inpx" }), std::inserter(result, result.end()), [&](const auto& item) { return QString("%1/%2").arg(folder, item); });
	return result;
}

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
		const auto id = GetActiveCollectionId();
		auto collection = FindCollectionById(id);
		assert(collection);
		return *collection;
	}

	bool ActiveCollectionExists() noexcept
	{
		const auto id = GetActiveCollectionId();
		const auto collection = FindCollectionById(id);
		return !!collection;
	}

	Collection* FindCollectionById(const QString& id) noexcept
	{
		const auto it = std::ranges::find(m_collections, id, [&](const auto& item) { return item->id; });
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
	Collections m_collections { CollectionImpl::Deserialize(*m_settings) };
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
	return std::ranges::any_of(GetCollections(), [&](const auto& item) { return item->name == name; });
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
