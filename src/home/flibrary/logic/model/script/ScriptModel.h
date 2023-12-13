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
	int	columnCount(const QModelIndex & /*parent*/ = QModelIndex()) const override;
	int	rowCount(const QModelIndex & /*parent*/ = QModelIndex()) const override;
	QVariant headerData(const int section, const Qt::Orientation orientation, const int role) const override;
	QVariant data(const QModelIndex & index, const int role) const override;
	bool setData(const QModelIndex & index, const QVariant & value, const int role) override;
	Qt::ItemFlags flags(const QModelIndex & index) const override;
	bool insertRows(const int row, const int count, const QModelIndex & parent) override;
	bool removeRows(const int row, const int count, const QModelIndex & /*parent*/) override;

private:
	PropagateConstPtr<class IScriptController, std::shared_ptr> m_scriptController;
	class ISourceModelObserver * m_observer { nullptr };
};

}
