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
	enum class Type
	{
		Xml,
		Html,
		Headless,
	};

	class XmlNodeGuard
	{
		NON_COPY_MOVABLE(XmlNodeGuard)

	public:
		XmlNodeGuard(XmlWriter& writer, const QString& name)
			: m_writer(writer)
		{
			(void)m_writer.WriteStartElement(name);
		}

		~XmlNodeGuard()
		{
			(void)m_writer.WriteEndElement();
		}

		XmlWriter* operator->() const noexcept
		{
			return &m_writer;
		}

	private:
		XmlWriter& m_writer;
	};

public:
	explicit XmlWriter(QIODevice& stream, Type type = Type::Xml, bool indented = true);
	~XmlWriter();

	XmlWriter& WriteProcessingInstruction(const QString& target, const QString& data);
	XmlWriter& WriteStartElement(const QString& name);
	XmlWriter& WriteStartElement(const QString& name, const XmlAttributes& attributes);
	XmlWriter& WriteEndElement();
	XmlWriter& WriteAttribute(const QString& name, const QString& value);
	XmlWriter& WriteCharacters(const QString& data);
	XmlWriter& CloseTag();

	XmlNodeGuard Guard(const QString& name);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Util
