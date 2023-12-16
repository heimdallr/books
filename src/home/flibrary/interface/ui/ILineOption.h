#pragma once

class QLineEdit;
class QString;

namespace HomeCompa::Flibrary {

class ILineOption  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ILineOption() = default;
	virtual void SetLineEdit(QLineEdit * lineEdit) noexcept = 0;
	virtual void SetSettingsKey(QString key, const QString & defaultValue) noexcept = 0;
};

}
