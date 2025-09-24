#pragma once

#include "interface/logic/IDataItem.h"

namespace HomeCompa::Flibrary
{

namespace IDataProviderDetails
{

class IDataProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Callback = std::function<void(IDataItem::Ptr)>;

public:
	virtual ~IDataProvider() = default;
	virtual void RequestBooks(bool force = false) const = 0;
	virtual const QString& GetNavigationID() const noexcept = 0;
};

}

class INavigationInfoProvider : virtual public IDataProviderDetails::IDataProvider
{
public:
	virtual void SetNavigationId(QString id) = 0;
	virtual void SetNavigationMode(enum class NavigationMode navigationMode) = 0;
	virtual void SetNavigationRequestCallback(Callback callback) = 0;
	virtual void RequestNavigation(bool force = false) const = 0;
};

class IBookInfoProvider : virtual public IDataProviderDetails::IDataProvider
{
public:
	virtual void SetBookRequestCallback(Callback callback) = 0;
	virtual void SetBooksViewMode(enum class ViewMode viewMode) = 0;
	virtual BookInfo GetBookInfo(long long id) const = 0;
};

class IAbstractDataProvider
	: public INavigationInfoProvider
	, public IBookInfoProvider
{
};

class IDataProvider : public IAbstractDataProvider
{
};

class IFilterDataProvider : public INavigationInfoProvider
{
};

} // namespace HomeCompa::Flibrary
