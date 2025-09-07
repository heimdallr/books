#pragma once

#include <QComboBox>

namespace HomeCompa::Flibrary
{

class FComboBox : public QComboBox
{
public:
	explicit FComboBox(QWidget* parent);

private: // QComboBox
	void showPopup() override;
};

} // namespace HomeCompa::Flibrary
