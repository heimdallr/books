#pragma once

#include <QWidget>

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class BaseDelegateEditor : public QWidget
{
protected:
	BaseDelegateEditor(QWidget * self, QWidget * parent);

public:
	virtual QWidget * GetWidget() const noexcept;
	virtual QString GetText() const = 0;
	virtual void SetText(const QString & value) = 0;
	virtual void SetParent(QWidget * parent);
	virtual void SetModel(QAbstractItemModel * model, const QModelIndex & index);

protected:
	QWidget * m_self;
	QAbstractItemModel * m_model { nullptr };
	int m_row { -1 };
	int m_column { -1 };
};

}
