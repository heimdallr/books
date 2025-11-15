#pragma once

#include <QPointer>

#include "interface/logic/IDataItem.h"
#include "interface/logic/IFilterProvider.h"

namespace HomeCompa::Flibrary
{

class IFilterController : public IFilterProvider
{
public:
	using Callback = std::function<void()>;

	class ICallback // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~ICallback() = default;

		virtual void OnStarted()     = 0;
		virtual void OnFinished()    = 0;
		virtual bool Stopped() const = 0;

		virtual std::weak_ptr<ICallback> Ptr() noexcept = 0;
	};

public:
	virtual void SetFilterEnabled(bool enabled)                                                                                                = 0;
	virtual void Apply()                                                                                                                       = 0;
	virtual void SetFlags(NavigationMode navigationMode, QString id, IDataItem::Flags flags)                                                   = 0;
	virtual void SetRating(const std::optional<int>& min, const std::optional<int>& max, bool hideUnrated)                                     = 0;
	virtual void SetNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback)   = 0;
	virtual void ClearNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) = 0;
	virtual void HideFiltered(NavigationMode navigationMode, QPointer<QAbstractItemModel> model, std::weak_ptr<ICallback> callback)            = 0;
};

} // namespace HomeCompa::Flibrary
