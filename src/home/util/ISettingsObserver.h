#pragma once

#include "fnd/observer.h"

class QString;
class QVariant;

namespace HomeCompa
{

class ISettingsObserver : public Observer
{
public:
	virtual void HandleValueChanged(const QString& key, const QVariant& value) = 0;
};

}
