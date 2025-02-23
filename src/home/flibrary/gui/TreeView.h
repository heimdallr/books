#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

class QAbstractItemView;
class QModelIndex;

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Flibrary
{

class TreeView final : public QWidget
{
	Q_OBJECT
	NON_COPY_MOVABLE(TreeView)

public:
	TreeView(std::shared_ptr<ISettings> settings,
	         std::shared_ptr<class IUiFactory> uiFactory,
	         std::shared_ptr<class ItemViewToolTipper> itemViewToolTipper,
	         std::shared_ptr<class ScrollBarController> scrollBarController,
	         std::shared_ptr<const class ICollectionProvider> collectionProvider,
	         QWidget* parent = nullptr);
	~TreeView() override;

signals:
	void NavigationModeNameChanged(QString navigationModeName) const;
	void ValueGeometryChanged(const QRect& geometry) const;

public:
	void SetNavigationModeName(QString navigationModeName);
	void ShowRemoved(bool hideRemoved);
	QAbstractItemView* GetView() const;
	void FillMenu(QMenu& menu);

private slots:
	void OnBookTitleToSearchVisibleChanged() const;

private: // QWidget
	void resizeEvent(QResizeEvent* event) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
