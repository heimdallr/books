#pragma once

#include <QObject>

namespace HomeCompa::Flibrary
{

class ItemViewToolTipper final : public QObject
{
public:
	explicit ItemViewToolTipper(QObject* parent = nullptr);

private: // QObject
	bool eventFilter(QObject* obj, QEvent* event) override;
};

}
