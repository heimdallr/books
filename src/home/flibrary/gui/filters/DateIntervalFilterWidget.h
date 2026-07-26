#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa
{

class ISettings;
class IParentWidgetProvider;

}

class QAbstractItemModel;

namespace HomeCompa::Flibrary
{

class DateIntervalFilterWidget final : public QWidget
{
	Q_OBJECT
	NON_COPY_MOVABLE(DateIntervalFilterWidget)

private:
	using Callback = std::function<void(bool, QVariantList)>;

public:
	DateIntervalFilterWidget(const QAbstractItemModel& model, int column, Callback callback, const IParentWidgetProvider& parentWidgetProvider, std::shared_ptr<ISettings> settings, QWidget* parent = nullptr);
	~DateIntervalFilterWidget() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
