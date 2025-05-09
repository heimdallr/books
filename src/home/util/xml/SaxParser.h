#pragma once

#include <QString>

#include "fnd/FindPair.h"
#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "export/Util.h"

class QIODevice;

namespace HomeCompa::Util
{

class XmlAttributes;

class UTIL_EXPORT SaxParser
{
	NON_COPY_MOVABLE(SaxParser)

public:
	struct PszComparerEndsWithCaseInsensitive
	{
		bool operator()(const std::string_view lhs, const std::string_view rhs) const
		{
			if (lhs.size() < rhs.size())
				return false;

			const auto *lp = lhs.data() + (lhs.size() - rhs.size()), *rp = rhs.data();
			while (*lp && *rp && std::tolower(*lp++) == std::tolower(*rp++))
				;

			return !*lp && !*rp;
		}
	};

protected:
	explicit SaxParser(QIODevice& stream, int64_t maxChunkSize = std::numeric_limits<int64_t>::max());
	virtual ~SaxParser();

protected:
	template <typename Obj, typename Value, size_t ArraySize, typename... ARGS>
	bool Parse(Obj& obj, Value (&array)[ArraySize], const QString& key, const ARGS&... args)
	{
		m_processed = true;
		const auto parser = FindSecond(array, key.toStdString().data(), &SaxParser::Stub<ARGS...>, PszComparerEndsWithCaseInsensitive {});
		return std::invoke(parser, obj, std::cref(args)...);
	}

	bool IsLastItemProcessed() const noexcept;

private:
	template <typename... ARGS>
	// ReSharper disable once CppMemberFunctionMayBeStatic
	bool Stub(const ARGS&...)
	{
		m_processed = false;
		return true;
	}

public:
	void Parse();

public:
	virtual bool OnProcessingInstruction(const QString& /*target*/, const QString& /*data*/)
	{
		return true;
	}

	virtual bool OnStartElement(const QString& name, const QString& path, const XmlAttributes& attributes);
	virtual bool OnEndElement(const QString& name, const QString& path);
	virtual bool OnCharacters(const QString& path, const QString& value);

	virtual bool OnWarning(const QString& text);
	virtual bool OnError(const QString& text);
	virtual bool OnFatalError(const QString& text);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;

protected:
	bool m_processed { true };
};

} // namespace HomeCompa::Util
