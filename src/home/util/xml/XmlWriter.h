#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "export/Util.h"

class QString;
class QIODevice;

namespace HomeCompa::Util
{

class XmlAttributes;

class UTIL_EXPORT XmlWriter
{
	NON_COPY_MOVABLE(XmlWriter)

public:
	explicit XmlWriter(QIODevice& stream, Type type = Type::Xml);
	~XmlWriter();

	XmlWriter& WriteProcessingInstruction(const QString& target, const QString& data);
	XmlWriter& WriteStartElement(const QString& name);
	XmlWriter& WriteStartElement(const QString& name, const XmlAttributes& attributes);
	XmlWriter& WriteEndElement();
	XmlWriter& WriteAttribute(const QString& name, const QString& value);
	XmlWriter& WriteCharacters(const QString& data);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Util
