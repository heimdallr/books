#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAnnotationController.h"
#include "interface/logic/IBookInteractor.h"
#include "interface/logic/IDataProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"

#include "AbstractTreeViewController.h"

class QString;

namespace HomeCompa::Flibrary
{

class TreeViewControllerBooks final : public AbstractTreeViewController
{
	NON_COPY_MOVABLE(TreeViewControllerBooks)

public:
	TreeViewControllerBooks(
		std::shared_ptr<ISettings>                  settings,
		const std::shared_ptr<IModelProvider>&      modelProvider,
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<IBookInfoProvider>          dataProvider,
		std::shared_ptr<const IBookInteractor>      bookInteractor,
		std::shared_ptr<IAnnotationController>      annotationController,
		std::shared_ptr<IDatabaseUser>              databaseUser
	);
	~TreeViewControllerBooks() override;

private: // ITreeViewController
	[[nodiscard]] std::vector<std::pair<const char*, int>> GetModeNames() const override;
	void                                                   SetCurrentId(ItemType type, QString id, bool force) override;
	const QString&                                         GetNavigationId() const noexcept override;

private: // AbstractTreeViewController
	void                   OnModeChanged(const QString& mode) override;
	[[nodiscard]] int      GetModeIndex(const QString& mode) const override;
	[[nodiscard]] ItemType GetItemType() const noexcept override;
	[[nodiscard]] QString  GetItemName() const override;
	[[nodiscard]] ViewMode GetViewMode() const noexcept override;
	void                   RequestContextMenu(const QModelIndex& index, RequestContextMenuOptions options, RequestContextMenuCallback callback) override;
	void                   OnContextMenuTriggered(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item) override;
	void                   OnDoubleClicked(const QModelIndex& index) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
