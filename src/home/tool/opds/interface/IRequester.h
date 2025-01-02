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

	virtual QByteArray GetRoot(const QString & self) const = 0;
	virtual QByteArray GetBookInfo(const QString & self, const QString & bookId) const = 0;
	virtual QByteArray GetCover(const QString & self, const QString & bookId) const = 0;
	virtual QByteArray GetCoverThumbnail(const QString & self, const QString & bookId) const = 0;
	virtual QByteArray GetBook(const QString & self, const QString & bookId) const = 0;
	virtual QByteArray GetBookZip(const QString & self, const QString & bookId) const = 0;

#define OPDS_ROOT_ITEM(NAME) virtual QByteArray Get##NAME##Navigation(const QString & self, const QString & value) const = 0;
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME) virtual QByteArray Get##NAME##Books(const QString & self, const QString & navigationId, const QString & value) const = 0;
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM
};

}
