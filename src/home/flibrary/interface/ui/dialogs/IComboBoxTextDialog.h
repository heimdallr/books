#pragma once

class QComboBox;
class QLineEdit;

namespace HomeCompa::Flibrary
{

class IComboBoxTextDialog // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IComboBoxTextDialog() = default;

public:
	virtual QDialog&   GetDialog()   = 0;
	virtual QComboBox& GetComboBox() = 0;
	virtual QLineEdit& GetLineEdit() = 0;
};

}
