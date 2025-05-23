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

class IPostProcessCallback // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IPostProcessCallback() = default;

	virtual QString GetFileName(const QString& bookId) const = 0;
	virtual std::pair<QString, std::vector<QByteArray>> GetAuthorInfo(const QString& name) const = 0;
};

}
