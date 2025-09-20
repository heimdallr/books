#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

#include "ItemViewToolTipper.h"
#include "ScrollBarController.h"

class QAbstractItemView;
class QModelIndex;

namespace HomeCompa::Flibrary
{

class TreeView final : public QWidget
{
	Q_OBJECT
	NON_COPY_MOVABLE(TreeView)

public:
	TreeView(std::shared_ptr<ISettings> settings,
	         std::shared_ptr<IUiFactory> uiFactory,
	         std::shared_ptr<ItemViewToolTipper> itemViewToolTipper,
	         std::shared_ptr<ScrollBarController> scrollBarController,
	         std::shared_ptr<const ICollectionProvider> collectionProvider,
	         QWidget* parent = nullptr);
	~TreeView() override;

signals:
	void NavigationModeNameChanged(QString navigationModeName) const;
	void ValueGeometryChanged(const QRect& geometry) const;
	void SearchNavigationItemSelected(long long id, const QString& text) const;

public:
	void SetNavigationModeName(QString navigationModeName);
	void ShowRemoved(bool showRemoved);
	QAbstractItemView* GetView() const;
	void SetMode(int mode, const QString& id);

private slots:
	void OnBookTitleToSearchVisibleChanged() const;

private: // QWidget
	void resizeEvent(QResizeEvent* event) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
