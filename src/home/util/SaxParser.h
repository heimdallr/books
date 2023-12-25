#pragma once

#include <QString>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "export/Util.h"

class QIODevice;

namespace HomeCompa::Util {

class UTIL_EXPORT SaxParser
{
	NON_COPY_MOVABLE(SaxParser)

public:
	class Attributes  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~Attributes() = default;
		virtual QString GetAttribute(const QString & key) const = 0;
	};

	struct PszComparerEndsWithCaseInsensitive
	{
		bool operator()(const std::string_view lhs, const std::string_view rhs) const
		{
			if (lhs.size() < rhs.size())
				return false;

			const auto * lp = lhs.data() + (lhs.size() - rhs.size()), * rp = rhs.data();
			while (*lp && *rp && std::tolower(*lp++) == std::tolower(*rp++))
				;

			return !*lp && !*rp;
		}
	};

protected:
	explicit SaxParser(QIODevice & stream);
	virtual ~SaxParser();

public:
	void Parse();

public:
	virtual bool OnStartElement(const QString & path, const Attributes & attributes) = 0;
	virtual bool OnEndElement(const QString & path) = 0;
	virtual bool OnCharacters(const QString & path, const QString & value) = 0;

	virtual bool OnWarning(const QString & text) = 0;
	virtual bool OnError(const QString & text) = 0;
	virtual bool OnFatalError(const QString & text) = 0;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
