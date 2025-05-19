#pragma once

namespace HomeCompa::ZipDetails
{

#define ZIP_FORMAT_ITEMS_X_MACRO \
	ZIP_FORMAT_ITEM(Zip)         \
	ZIP_FORMAT_ITEM(SevenZip)    \
	ZIP_FORMAT_ITEM(BZip2)       \
	ZIP_FORMAT_ITEM(Xz)          \
	ZIP_FORMAT_ITEM(Wim)         \
	ZIP_FORMAT_ITEM(Tar)         \
	ZIP_FORMAT_ITEM(GZip)

enum class Format
{
	Auto,
#define ZIP_FORMAT_ITEM(NAME) NAME,
	ZIP_FORMAT_ITEMS_X_MACRO
#undef ZIP_FORMAT_ITEM
};

}
