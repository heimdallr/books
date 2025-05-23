#pragma once

#include <functional>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAuthorAnnotationController.h"
#include "interface/logic/IDataItem.h"
#include "interface/logic/IDataProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/INavigationQueryExecutor.h"

class QString;

namespace HomeCompa::Flibrary
{

class DataProvider : public IDataProvider
{
	NON_COPY_MOVABLE(DataProvider)

public:
	DataProvider(std::shared_ptr<IDatabaseUser> databaseUser, std::shared_ptr<INavigationQueryExecutor> navigationQueryExecutor, std::shared_ptr<IAuthorAnnotationController> authorAnnotationController);
	~DataProvider() override;

public:
	void SetNavigationId(QString id) override;
	void SetNavigationMode(NavigationMode navigationMode) override;
	void SetNavigationRequestCallback(Callback callback) override;
	void RequestNavigation(bool force) const override;
	void RequestBooks(bool force) const override;

private: // IBookInfoProvider
	void SetBookRequestCallback(Callback callback) override;
	void SetBooksViewMode(enum class ViewMode viewMode) override;
	BookInfo GetBookInfo(long long id) const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
