#pragma once

#include <functional>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "DataItem.h"

class QString;

namespace HomeCompa::Flibrary {

class DataProvider
{
	NON_COPY_MOVABLE(DataProvider)

public:
	using Callback = std::function<void(DataItem::Ptr)>;

public:
	explicit DataProvider(std::shared_ptr<class ILogicFactory> logicFactory);
	~DataProvider();

public:
	void SetNavigationId(QString id);
	void SetNavigationMode(enum class NavigationMode navigationMode);
	void SetBooksViewMode(enum class ViewMode viewMode);

	void SetNavigationRequestCallback(Callback callback);
	void SetBookRequestCallback(Callback callback);

	void RequestBooks() const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
