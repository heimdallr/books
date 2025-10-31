#pragma once

#include <QTreeView>

namespace HomeCompa::Flibrary
{

class CustomTreeView final : public QTreeView
{
public:
	explicit CustomTreeView(QWidget* parent = nullptr);

public:
	void UpdateSectionSize();
};

}
