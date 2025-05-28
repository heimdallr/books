#pragma once

class QString;
class QVariant;

namespace HomeCompa::Flibrary
{

class ILibRateProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ILibRateProvider() = default;
	virtual QVariant GetLibRate(const QString& libId, const QString& libRate) const = 0;
	virtual QVariant GetForegroundBrush(const QString& libId, const QString& libRate) const = 0;
};

}
