#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

class QAbstractItemModel;

namespace HomeCompa
{

class IParentWidgetProvider;
class ISettings;

}

namespace HomeCompa::Flibrary
{

class RangeFilterWidget final : public QWidget
{
	Q_OBJECT
	NON_COPY_MOVABLE(RangeFilterWidget)

private:
	using Callback = std::function<void(bool, QVariantList)>;

public:
	RangeFilterWidget(const QAbstractItemModel& model, int column, Callback callback, const IParentWidgetProvider& parentWidgetProvider, std::shared_ptr<ISettings> settings, QWidget* parent = nullptr);
	~RangeFilterWidget() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
