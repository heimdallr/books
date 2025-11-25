#pragma once

class QString;
class QVariant;

namespace HomeCompa::Flibrary
{

class ILibRateProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ILibRateProvider()                                                         = default;
	virtual double   GetLibRate(long long bookId, const QString& libRate) const         = 0;
	virtual QVariant GetLibRateString(long long bookId, const QString& libRate) const   = 0;
	virtual QVariant GetForegroundBrush(long long bookId, const QString& libRate) const = 0;
};

}
