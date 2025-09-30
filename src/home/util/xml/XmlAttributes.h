#pragma once

class QString;

namespace HomeCompa::Util
{

class XmlAttributes // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~XmlAttributes()                               = default;
	virtual QString GetAttribute(const QString& key) const = 0;
	virtual size_t  GetCount() const                       = 0;
	virtual QString GetName(size_t index) const            = 0;
	virtual QString GetValue(size_t index) const           = 0;
};

}
