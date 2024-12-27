#include "CollectionProvider.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimer>

#include <plog/Log.h>

#include "fnd/observable.h"
#include "fnd/ScopedCall.h"

#include "CollectionImpl.h"

#include "inpx/src/util/inpx.h"
#include "util/hash.h"
#include "util/IExecutor.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

QString GetInpxImpl(const QString & folder)
{
	const auto inpxList = QDir(folder).entryList({ "*.inpx" });
	return inpxList.isEmpty() ? QString{} : QString("%1/%2").arg(folder, inpxList.front());
}

}

class CollectionProvider::Impl final
	: public Observable<ICollectionsObserver>
{
public:
	explicit Impl(std::shared_ptr<ISettings> settings)
		: m_settings { std::move(settings) }
	{
	}

	Collections & GetCollections() noexcept
	{
		return m_collections;
	}

	const Collection& GetActiveCollection() const noexcept
	{
		const auto collection = FindCollectionById(GetActiveCollectionId());
		assert(collection);
		return *collection;
	}

	bool ActiveCollectionExists() const noexcept
	{
		const auto collection = FindCollectionById(GetActiveCollectionId());
		return !!collection;
	}

	const Collection* FindCollectionById(const QString & id) const noexcept
	{
		const auto it = std::ranges::find(m_collections, id, [&] (const auto & item) { return item->id; });
		if (it == std::cend(m_collections))
			return nullptr;

		const auto & collection = **it;
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
	: m_impl(std::move(settings))
{
	PLOGD << "CollectionProvider created";
}

CollectionProvider::~CollectionProvider()
{
	PLOGD << "CollectionProvider destroyed";
}

bool CollectionProvider::IsEmpty() const noexcept
{
	return GetCollections().empty();
}

bool CollectionProvider::IsCollectionNameExists(const QString & name) const
{
	return std::ranges::any_of(GetCollections(), [&] (const auto & item)
	{
		return item->name == name;
	});
}

QString CollectionProvider::GetCollectionDatabaseName(const QString & databaseFileName) const
{
	const auto collection = m_impl->FindCollectionById(Util::md5(databaseFileName.toUtf8()));
	return collection ? collection->name : QString{};
}

QString CollectionProvider::GetInpx(const QString & folder) const
{
	return GetInpxImpl(folder);
}

bool CollectionProvider::IsCollectionFolderHasInpx(const QString & folder) const
{
	return !GetInpx(folder).isEmpty();
}

Collections & CollectionProvider::GetCollections() noexcept
{
	return m_impl->GetCollections();
}

const Collections & CollectionProvider::GetCollections() const noexcept
{
	return const_cast<Impl&>(*m_impl).GetCollections();
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

void CollectionProvider::RegisterObserver(ICollectionsObserver * observer)
{
	m_impl->Register(observer);
}

void CollectionProvider::UnregisterObserver(ICollectionsObserver * observer)
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