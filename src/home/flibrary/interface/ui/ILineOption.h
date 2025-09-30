#pragma once

#include "fnd/observer.h"

class QLineEdit;
class QString;

namespace HomeCompa::Flibrary
{

class ILineOption // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnOptionEditing(const QString& value)         = 0;
		virtual void OnOptionEditingFinished(const QString& value) = 0;
	};

public:
	virtual ~ILineOption()                                                         = default;
	virtual void SetLineEdit(QLineEdit* lineEdit) noexcept                         = 0;
	virtual void SetSettingsKey(QString key, const QString& defaultValue) noexcept = 0;

	virtual void Register(IObserver* observer)   = 0;
	virtual void Unregister(IObserver* observer) = 0;
};

}
