#pragma once

#include "fnd/Lockable.h"

#include "interface/logic/IDataItem.h"

#include "ITreeViewController.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary
{
class ILibRateProvider;

class IModelProvider
	: public ITreeViewController::IObserver
	, public Lockable<IModelProvider>
{
public:
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateListModel(IDataItem::Ptr data, bool autoAcceptChildRows) const        = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateTreeModel(IDataItem::Ptr data, bool autoAcceptChildRows) const        = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateBookListModel(IDataItem::Ptr data, bool autoAcceptChildRows) const    = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateBookTreeModel(IDataItem::Ptr data, bool autoAcceptChildRows) const    = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateAuthorsListModel(IDataItem::Ptr data, bool autoAcceptChildRows) const = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateSearchListModel(IDataItem::Ptr data, bool autoAcceptChildRows) const  = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateScriptModel() const                                                   = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateScriptCommandModel() const                                            = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateAuthorReviewModel() const                                             = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateFilterListModel(IDataItem::Ptr data) const                            = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     CreateFilterTreeModel(IDataItem::Ptr data) const                            = 0;
	[[nodiscard]] virtual IDataItem::Ptr                          GetData() const noexcept                                                    = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel>     GetSourceModel() const noexcept                                             = 0;
	[[nodiscard]] virtual std::shared_ptr<const ILibRateProvider> GetLibRateProvider() const                                                  = 0;
};

} // namespace HomeCompa::Flibrary
