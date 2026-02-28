#pragma once
#include "IRequester.h"

namespace HomeCompa
{
class ISettings;
}

class QByteArray;
class QString;

namespace HomeCompa::Opds
{

class INoSqlRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	static constexpr auto CONVERTERS_ROOT     = "Preferences/opds/converters";
	static constexpr auto CONVERTER_ROOT      = "Preferences/opds/converter";
	static constexpr auto CONVERTER_COMMAND   = "command";
	static constexpr auto CONVERTER_ARGUMENTS = "arguments";
	static constexpr auto CONVERTER_CWD       = "cwd";
	static constexpr auto CONVERTER_EXT       = "ext";
	static constexpr auto CONVERTER_PROFILE   = "profile";
	static constexpr auto CONVERTER_TITLE     = "title";

public:
	virtual ~INoSqlRequester() = default;

	virtual QByteArray                     GetCover(const QString& bookId) const                                                                             = 0;
	virtual QByteArray                     GetCoverThumbnail(const QString& bookId) const                                                                    = 0;
	virtual std::pair<QString, QByteArray> GetBook(const QString& bookId, bool restoreImages = true, const IRequester::Parameters& parameters = {}) const    = 0;
	virtual std::pair<QString, QByteArray> GetBookZip(const QString& bookId, bool restoreImages = true, const IRequester::Parameters& parameters = {}) const = 0;

	static QString GetProfileRoot(const ISettings& settings, const QString& profileTitle);
};

} // namespace HomeCompa::Opds
