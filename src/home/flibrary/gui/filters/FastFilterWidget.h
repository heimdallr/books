#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa::Util
{

class ItemViewToolTipper;
class ScrollBarController;

}

namespace HomeCompa
{

class IParentWidgetProvider;
class ISettings;

}

namespace HomeCompa::Flibrary
{

class FastFilterWidget final : public QWidget
{
	NON_COPY_MOVABLE(FastFilterWidget)

private:
	using Callback = std::function<void(bool, QVariantList)>;

public:
	FastFilterWidget(
		const QAbstractItemModel&                  model,
		int                                        column,
		Callback                                   callback,
		const IParentWidgetProvider&               parentWidgetProvider,
		const ISettings&                           settings,
		std::shared_ptr<Util::ItemViewToolTipper>  toolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarController,
		QWidget*                                   parent = nullptr
	);
	~FastFilterWidget() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
