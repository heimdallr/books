#pragma once

#include <QAbstractTableModel>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

class ScriptModel final
	: public QAbstractTableModel
{
	NON_COPY_MOVABLE(ScriptModel)

public:
	explicit ScriptModel(std::shared_ptr<class IScriptController> scriptController, QObject * parent = nullptr);
	~ScriptModel() override;

private: // QAbstractItemModel
	int	columnCount(const QModelIndex & parent) const override;
	int	rowCount(const QModelIndex & parent) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	QVariant data(const QModelIndex & index, int role) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;
	Qt::ItemFlags flags(const QModelIndex & index) const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
