#pragma once

class QString;
class QByteArray;

namespace HomeCompa::Opds
{

class ICoverCache // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ICoverCache() = default;

	virtual void       Set(QString id, QByteArray data) const = 0;
	virtual QByteArray Get(const QString& id) const           = 0;
};

}
