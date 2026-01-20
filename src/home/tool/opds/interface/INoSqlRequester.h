#pragma once

class QByteArray;
class QString;

namespace HomeCompa::Opds
{

class INoSqlRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	static constexpr auto CONVERTER_COMMAND   = "Preferences/opds/converter/command";
	static constexpr auto CONVERTER_ARGUMENTS = "Preferences/opds/converter/arguments";
	static constexpr auto CONVERTER_CWD       = "Preferences/opds/converter/cwd";
	static constexpr auto CONVERTER_EXT       = "Preferences/opds/converter/ext";

public:
	virtual ~INoSqlRequester() = default;

	virtual QByteArray                     GetCover(const QString& bookId) const                              = 0;
	virtual QByteArray                     GetCoverThumbnail(const QString& bookId) const                     = 0;
	virtual std::pair<QString, QByteArray> GetBook(const QString& bookId, bool restoreImages = true) const    = 0;
	virtual std::pair<QString, QByteArray> GetBookZip(const QString& bookId, bool restoreImages = true) const = 0;
};

}
