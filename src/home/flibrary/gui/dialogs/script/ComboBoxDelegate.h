#pragma once

#include <QStyledItemDelegate>

namespace HomeCompa::Flibrary
{

class ComboBoxDelegate : public QStyledItemDelegate
{
public:
	using Values = std::vector<std::pair<int, QString>>;

public:
	explicit ComboBoxDelegate(QObject* parent = nullptr);

public: // IComboBoxDelegate
	void SetValues(Values values);

private: // QStyledItemDelegate
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void     setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void     setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

private:
	Values m_values;
};

class ScriptComboBoxDelegate : virtual public ComboBoxDelegate
{
};

class CommandComboBoxDelegate : virtual public ComboBoxDelegate
{
};

} // namespace HomeCompa::Flibrary
