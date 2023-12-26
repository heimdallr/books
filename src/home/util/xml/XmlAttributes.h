#pragma once

class QString;

namespace HomeCompa::Util {

class XmlAttributes  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~XmlAttributes() = default;
	virtual QString GetAttribute(const QString & key) const = 0;
};

}
