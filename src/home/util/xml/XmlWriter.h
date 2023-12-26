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

	void WriteProcessingInstruction(const QString & target, const QString & data);
	void WriteStartElement(const QString & name, const XmlAttributes & attributes);
	void WriteEndElement(const QString & name);
	void WriteCharacters(const QString & data);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
