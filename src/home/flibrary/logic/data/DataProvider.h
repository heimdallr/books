#pragma once

#include <functional>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IDataItem.h"

class QString;

namespace HomeCompa::Flibrary {

class DataProvider
{
	NON_COPY_MOVABLE(DataProvider)

public:
	using Callback = std::function<void(IDataItem::Ptr)>;

public:
	DataProvider(std::shared_ptr<class IDatabaseUser> databaseUser
		, std::shared_ptr<class INavigationQueryExecutor> navigationQueryExecutor
	);
	~DataProvider();

public:
	void SetNavigationId(QString id);
	void SetNavigationMode(enum class NavigationMode navigationMode);
	void SetBooksViewMode(enum class ViewMode viewMode);

	void SetNavigationRequestCallback(Callback callback);
	void SetBookRequestCallback(Callback callback);

	void RequestNavigation(bool force = false) const;
	void RequestBooks(bool force = false) const;

	BookInfo GetBookInfo(long long id) const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
