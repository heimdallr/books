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
	explicit ScriptModel(const std::shared_ptr<class IScriptControllerProvider> & scriptControllerProvider, QObject * parent = nullptr);
	~ScriptModel() override;

private: // QAbstractItemModel
	int	columnCount(const QModelIndex & parent = QModelIndex()) const override;
	int	rowCount(const QModelIndex & parent = QModelIndex()) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	QVariant data(const QModelIndex & index, int role) const override;
	bool setData(const QModelIndex & index, const QVariant & value, int role) override;
	Qt::ItemFlags flags(const QModelIndex & index) const override;
	bool insertRows(int row, int count, const QModelIndex & parent) override;
	bool removeRows(int row, int count, const QModelIndex & parent) override;

private:
	PropagateConstPtr<class IScriptController, std::shared_ptr> m_scriptController;
	class ISourceModelObserver * m_observer { nullptr };
};

}
