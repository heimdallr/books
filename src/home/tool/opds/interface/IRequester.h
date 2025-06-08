#pragma once

#include <QString>

class QByteArray;

#define OPDS_ROOT_ITEMS_X_MACRO \
	OPDS_ROOT_ITEM(Authors)     \
	OPDS_ROOT_ITEM(Series)      \
	OPDS_ROOT_ITEM(Genres)      \
	OPDS_ROOT_ITEM(Keywords)    \
	OPDS_ROOT_ITEM(Archives)    \
	OPDS_ROOT_ITEM(Groups)

namespace HomeCompa::Opds
{

class IRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Parameters = std::unordered_map<QString, QString>;

public:
	virtual ~IRequester() = default;

	virtual QByteArray GetRoot(const QString& root, const Parameters& parameters) const = 0;
	virtual QByteArray GetBooks(const QString& root, const Parameters& parameters) const = 0;

#define OPDS_ROOT_ITEM(NAME) virtual QByteArray Get##NAME(const QString& root, const Parameters& parameters) const = 0;
	OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM
};

} // namespace HomeCompa::Opds
