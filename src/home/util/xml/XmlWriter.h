#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "export/Util.h"

class QString;
class QIODevice;

namespace HomeCompa::Util {

class XmlAttributes;

class UTIL_EXPORT XmlWriter
{
	NON_COPY_MOVABLE(XmlWriter)

public:
	explicit XmlWriter(QIODevice & stream);
	~XmlWriter();

	XmlWriter & WriteProcessingInstruction(const QString & target, const QString & data);
	XmlWriter & WriteStartElement(const QString & name);
	XmlWriter & WriteStartElement(const QString & name, const XmlAttributes & attributes);
	XmlWriter & WriteEndElement(const QString & name);
	XmlWriter & WriteAttribute(const QString & name, const QString & value);
	XmlWriter & WriteCharacters(const QString & data);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
