#pragma once

#include <QStyledItemDelegate>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractScrollArea;

namespace HomeCompa::Flibrary {

class TreeViewDelegateBooks final : public QStyledItemDelegate
{
	NON_COPY_MOVABLE(TreeViewDelegateBooks)

public:
	using TextDelegate = QString(*)(const QVariant & value);

public:
	TreeViewDelegateBooks(const std::shared_ptr<const class IUiFactory> & uiFactory
		, std::shared_ptr<const class IRateStarsProvider> rateStarsProvider
		, QObject * parent = nullptr
	);
	~TreeViewDelegateBooks() override;

private: // QStyledItemDelegate
	void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
	QString displayText(const QVariant & value, const QLocale & /*locale*/) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
