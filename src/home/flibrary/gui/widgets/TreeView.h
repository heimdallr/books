#pragma once

#include <QTreeView>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa {

class ISettings;

}

namespace HomeCompa::Flibrary {

class CustomTreeView final : public QTreeView
{
	NON_COPY_MOVABLE(CustomTreeView)

public:
	explicit CustomTreeView(QWidget* parent = nullptr);
	~CustomTreeView() override;

private: // QTreeView
	void drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const override;

public:
	void Init(std::shared_ptr<const ISettings> settings);
	void OnModelChanged(const QAbstractItemModel& model);
	void UpdateSectionSize();
	void SetIsNavigation() noexcept;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
