#pragma once

class QByteArray;
class QString;

#define OPDS_ROOT_ITEMS_X_MACRO   \
		OPDS_ROOT_ITEM(Authors  ) \
		OPDS_ROOT_ITEM(Series   ) \
		OPDS_ROOT_ITEM(Genres   ) \
		OPDS_ROOT_ITEM(Keywords ) \
		OPDS_ROOT_ITEM(Archives ) \
		OPDS_ROOT_ITEM(Groups   )

namespace HomeCompa::Opds {

class IRequester  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IRequester() = default;

	virtual QByteArray GetRoot() const = 0;

#define OPDS_ROOT_ITEM(NAME) virtual QByteArray Get##NAME() const = 0;
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME) virtual QByteArray Get##NAME##StartsWith(const QString & value) const = 0;
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM
};

}
