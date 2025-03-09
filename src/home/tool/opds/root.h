#pragma once

namespace HomeCompa::Opds
{

#define OPDS_REQUEST_ROOT_ITEMS_X_MACRO \
	OPDS_REQUEST_ROOT_ITEM(opds)        \
	OPDS_REQUEST_ROOT_ITEM(web)

enum class ContentType
{
	Root,
	BookInfo,
	Navigation,
	Authors,
	Books,
	BookText,
};

}
