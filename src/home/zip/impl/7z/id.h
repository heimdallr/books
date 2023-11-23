#pragma once

struct _GUID;

namespace HomeCompa::Zip::Impl::SevenZip {

#define ARCHIVE_FORMAT_ITEMS_X_MACRO \
		ARCHIVE_FORMAT_ITEM(Zip)     \
		ARCHIVE_FORMAT_ITEM(SevenZip)\
		ARCHIVE_FORMAT_ITEM(Rar)     \
		ARCHIVE_FORMAT_ITEM(GZip)    \
		ARCHIVE_FORMAT_ITEM(BZip2)   \
		ARCHIVE_FORMAT_ITEM(Tar)     \
		ARCHIVE_FORMAT_ITEM(Lzma)    \
		ARCHIVE_FORMAT_ITEM(Lzma86)  \
		ARCHIVE_FORMAT_ITEM(Cab)     \
		ARCHIVE_FORMAT_ITEM(Iso)     \
		ARCHIVE_FORMAT_ITEM(Arj)     \
		ARCHIVE_FORMAT_ITEM(XZ)

enum Format
{
		Unknown = -1,
#define ARCHIVE_FORMAT_ITEM(NAME) NAME,
		ARCHIVE_FORMAT_ITEMS_X_MACRO
#undef  ARCHIVE_FORMAT_ITEM
};

const _GUID * GetCompressionGUID(Format format);

}
