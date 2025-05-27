#pragma once

class QString;

namespace HomeCompa::Flibrary
{

class ILibRateProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ILibRateProvider() = default;
	virtual QString GetLibRate(const QString& libId, QString libRate) const = 0;
};

}
