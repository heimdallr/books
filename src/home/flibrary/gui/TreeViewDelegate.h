#pragma once

#include <QStyledItemDelegate>

class QAbstractScrollArea;

namespace HomeCompa::Flibrary {

class TreeViewDelegateBooks final : public QStyledItemDelegate
{
public:
	using TextDelegate = QString(*)(const QVariant & value);

public:
	explicit TreeViewDelegateBooks(const std::shared_ptr<class IUiFactory> & uiFactory, QObject * parent = nullptr);
	~TreeViewDelegateBooks() override;

private: // QStyledItemDelegate
	void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
	QString displayText(const QVariant & value, const QLocale & /*locale*/) const override;

private:
	QWidget & m_view;
	mutable TextDelegate m_textDelegate;
};

}
