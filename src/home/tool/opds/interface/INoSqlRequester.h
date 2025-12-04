#pragma once

class QByteArray;
class QString;

namespace HomeCompa::Opds
{

class INoSqlRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~INoSqlRequester() = default;

	virtual QByteArray                     GetCover(const QString& bookId) const                              = 0;
	virtual QByteArray                     GetCoverThumbnail(const QString& bookId) const                     = 0;
	virtual std::pair<QString, QByteArray> GetBook(const QString& bookId, bool restoreImages = true) const    = 0;
	virtual std::pair<QString, QByteArray> GetBookZip(const QString& bookId, bool restoreImages = true) const = 0;
};

}
