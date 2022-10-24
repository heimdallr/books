#pragma once

#include "fnd/memory.h"

#include "UtilLib.h"

class QString;
class QVariant;

namespace HomeCompa {

class UTIL_API Settings
{
public:
	Settings(const QString & organization, const QString & application);
	~Settings();

public:
	QVariant Get(const QString & key, const QVariant & defaultValue) const;
	void Set(const QString & key, const QVariant & value);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
