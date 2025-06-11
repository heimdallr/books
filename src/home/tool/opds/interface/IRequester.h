#pragma once

#include <QString>

class QByteArray;

#define OPDS_NAVIGATION_ITEMS_X_MACRO \
	OPDS_INVOKER_ITEM(Authors)        \
	OPDS_INVOKER_ITEM(Series)         \
	OPDS_INVOKER_ITEM(Genres)         \
	OPDS_INVOKER_ITEM(Keywords)       \
	OPDS_INVOKER_ITEM(Updates)        \
	OPDS_INVOKER_ITEM(Archives)       \
	OPDS_INVOKER_ITEM(Groups)

#define OPDS_ADDITIONAL_ITEMS_X_MACRO \
	OPDS_INVOKER_ITEM(Root)           \
	OPDS_INVOKER_ITEM(Books)          \
	OPDS_INVOKER_ITEM(BookInfo)

namespace HomeCompa::Opds
{

class IRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Parameters = std::unordered_map<QString, QString>;

public:
	virtual ~IRequester() = default;
	virtual QByteArray Search(const QString& root, const Parameters& parameters) const = 0;

#define OPDS_INVOKER_ITEM(NAME) virtual QByteArray Get##NAME(const QString& root, const Parameters& parameters) const = 0;
	OPDS_NAVIGATION_ITEMS_X_MACRO
	OPDS_ADDITIONAL_ITEMS_X_MACRO
#undef OPDS_INVOKER_ITEM
};

} // namespace HomeCompa::Opds
