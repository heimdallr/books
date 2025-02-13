#pragma once

#include <QObject>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Flibrary
{

class ItemViewToolTipper final : public QObject
{
	NON_COPY_MOVABLE(ItemViewToolTipper)

public:
	explicit ItemViewToolTipper(std::shared_ptr<ISettings> settings, QObject* parent = nullptr);
	~ItemViewToolTipper() override;

private: // QObject
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
