#pragma once

#include <QIdentityProxyModel>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDataItem.h"
#include "interface/logic/IModelProvider.h"

namespace HomeCompa::Flibrary
{

class AbstractFilteredProxyModel : public QIdentityProxyModel
{
protected:
	explicit AbstractFilteredProxyModel(QObject* parent = nullptr);
};

class FilteredProxyModel final : public AbstractFilteredProxyModel
{
	NON_COPY_MOVABLE(FilteredProxyModel)

public:
	explicit FilteredProxyModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent = nullptr);
	~FilteredProxyModel() override;

private: // QAbstractItemModel
	QVariant data(const QModelIndex& index, int role) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private:
	void Check(const QModelIndex& parent, Qt::CheckState state);
	bool Check(const QVariant& value, const std::function<bool(const QModelIndex&)>& f) const;
	bool Check(const QVariant& value, Qt::CheckState checkState);
	QStringList CollectLanguages() const;
	int GetCount() const;
	void GetSelected(const QModelIndex& index, const QModelIndexList& indexList, QModelIndexList* selected) const;

private:
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_sourceModel;
};

} // namespace HomeCompa::Flibrary
