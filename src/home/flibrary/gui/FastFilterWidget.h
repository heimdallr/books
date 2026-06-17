#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa
{
class IParentWidgetProvider;
}

namespace HomeCompa::Flibrary
{

class FastFilterWidget final : public QWidget
{
	NON_COPY_MOVABLE(FastFilterWidget)

public:
	using Callback = std::function<void(bool, QVariantList)>;

public:
	FastFilterWidget(const QAbstractItemModel& model, int column, Callback callback, const IParentWidgetProvider& parentWidgetProvider, QWidget* parent = nullptr);
	~FastFilterWidget() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
