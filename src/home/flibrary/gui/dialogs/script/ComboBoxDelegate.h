#pragma once

#include <QStyledItemDelegate>

namespace HomeCompa::Flibrary {

class IComboBoxDelegate : public QStyledItemDelegate
{
public:
	using Values = std::vector<std::pair<int, QString>>;

public:
	virtual void SetValues(Values values) = 0;
};

class ScriptComboBoxDelegate : virtual public IComboBoxDelegate{};
class CommandComboBoxDelegate : virtual public IComboBoxDelegate{};

class ComboBoxDelegate
	: public ScriptComboBoxDelegate
	, public CommandComboBoxDelegate
{
private: // QStyledItemDelegate
	QWidget * createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
	void setEditorData(QWidget * editor, const QModelIndex & index) const override;
	void setModelData(QWidget * editor, QAbstractItemModel * model, const QModelIndex & index) const override;

private: // IComboBoxDelegate
	void SetValues(Values values) override;

private:
	Values m_values;
};

}
