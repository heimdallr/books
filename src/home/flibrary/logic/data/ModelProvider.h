#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IModelProvider.h"

class QAbstractItemModel;

namespace Hypodermic
{
class Container;
}

namespace HomeCompa::Flibrary
{

class ModelProvider final : public IModelProvider
{
	NON_COPY_MOVABLE(ModelProvider)

public:
	explicit ModelProvider(Hypodermic::Container& container);
	~ModelProvider() override;

private: // IModelProvider
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateListModel(IDataItem::Ptr data, bool autoAcceptChildRows) const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateTreeModel(IDataItem::Ptr data, bool autoAcceptChildRows) const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateBookListModel(IDataItem::Ptr data, bool autoAcceptChildRows) const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateBookTreeModel(IDataItem::Ptr data, bool autoAcceptChildRows) const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateAuthorsListModel(IDataItem::Ptr data, bool autoAcceptChildRows) const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateSearchListModel(IDataItem::Ptr data, bool autoAcceptChildRows) const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateScriptModel() const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateScriptCommandModel() const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateAuthorReviewModel() const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateFilterListModel(IDataItem::Ptr data) const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateFilterTreeModel(IDataItem::Ptr data) const override;
	[[nodiscard]] IDataItem::Ptr GetData() const noexcept override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> GetSourceModel() const noexcept override;
	[[nodiscard]] std::shared_ptr<const ILibRateProvider> GetLibRateProvider() const override;

private: // ITreeViewController::IObserver
	void OnModeChanged(int index) override;
	void OnModelChanged(QAbstractItemModel* model) override;
	void OnContextMenuTriggered(const QString& id, const IDataItem::Ptr& item) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
