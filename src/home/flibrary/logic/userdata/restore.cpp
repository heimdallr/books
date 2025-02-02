#include <QFile>
#include <QString>

#include <plog/Log.h>

#include "fnd/EnumBitmask.h"

#include "util/IExecutor.h"
#include "util/xml/SaxParser.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"

#include "restore.h"

#include "util/xml/XmlAttributes.h"

namespace HomeCompa::Flibrary::UserData {

enum class Check
{
	None                          = 0,
	RootNodeFound                 = 1 << 0,
	VersionNodeFound              = 1 << 1,
	VersionAttributeFound         = 1 << 2,
	VersionAttributeMustBeInteger = 1 << 3,
	VersionNumberMustBeActual     = 1 << 4,
	UserDataNodeFound             = 1 << 5,
};

}

ENABLE_BITMASK_OPERATORS(HomeCompa::Flibrary::UserData::Check);

#define RESTORE_ITEMS_X_MACRO        \
		RESTORE_ITEM(Books, 1)       \
		RESTORE_ITEM(Books, 2)       \
		RESTORE_ITEM(Books, 3)       \
		RESTORE_ITEM(Groups, 1)      \
		RESTORE_ITEM(Groups, 3)      \
		RESTORE_ITEM(Searches, 1)    \
		RESTORE_ITEM(Searches, 3)    \
		RESTORE_ITEM(Searches, 5)    \
		RESTORE_ITEM(ExportStat, 4)

namespace HomeCompa::Flibrary::UserData {

#define RESTORE_ITEM(NAME, VERSION) std::unique_ptr<IRestorer> Create##NAME##Restorer##VERSION();
RESTORE_ITEMS_X_MACRO
#undef	RESTORE_ITEM

namespace {

constexpr auto CONTEXT = "UserData";
constexpr auto CANNOT_READ_FROM = QT_TRANSLATE_NOOP("UserData", "Cannot read from %1");

TR_DEF

constexpr auto FLIBRARY_BACKUP = "FlibraryBackup";
constexpr auto FLIBRARY_BACKUP_VERSION = "FlibraryBackup/FlibraryBackupVersion";
constexpr auto FLIBRARY_BACKUP_USER_DATA = "FlibraryBackup/FlibraryUserData";
constexpr auto FLIBRARY_BACKUP_USER_DATA_BOOKS = "FlibraryBackup/FlibraryUserData/Books";
constexpr auto FLIBRARY_BACKUP_USER_DATA_GROUPS = "FlibraryBackup/FlibraryUserData/Groups";
constexpr auto FLIBRARY_BACKUP_USER_DATA_SEARCHES = "FlibraryBackup/FlibraryUserData/Searches";
constexpr auto FLIBRARY_BACKUP_USER_DATA_EXPORT_STAT = "FlibraryBackup/FlibraryUserData/ExportStat";

class XmlParser final
	: public Util::SaxParser
{
	NON_COPY_MOVABLE(XmlParser)

public:
	explicit XmlParser(QIODevice & stream, DB::IDatabase & db)
		: SaxParser(stream)
	{
		constexpr std::tuple<const char *, int, RestorerCreator> creators[] {
#define RESTORE_ITEM(NAME, VERSION) { #NAME, VERSION, &Create##NAME##Restorer##VERSION },
		RESTORE_ITEMS_X_MACRO
#undef	RESTORE_ITEM
		};
		for (const auto & [name, version, creator] : creators)
			m_restorerCreators[name].try_emplace(version, creator);

		Parse();
		Validate();
		if (!m_error.isEmpty())
			return;

		for (const auto & restorer : m_restorers)
			restorer->Restore(db);
	}

	~XmlParser() override = default;

	const QString & GetError() const noexcept
	{
		return m_error;
	}

private: // Util::SaxParser
	bool OnStartElement(const QString & name, const QString & path, const Util::XmlAttributes & attributes) override
	{
		using ParseElementFunction = bool(XmlParser::*)(const QString &, const Util::XmlAttributes &);
		using ParseElementItem = std::pair<const char *, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[]
		{
			{ FLIBRARY_BACKUP                      , &XmlParser::OnStartElementFlibraryBackup },
			{ FLIBRARY_BACKUP_VERSION              , &XmlParser::OnStartElementFlibraryBackupVersion },
			{ FLIBRARY_BACKUP_USER_DATA            , &XmlParser::OnStartElementFlibraryBackupUserData },
			{ FLIBRARY_BACKUP_USER_DATA_BOOKS      , &XmlParser::OnStartElementFlibraryBackupUserDataItem },
			{ FLIBRARY_BACKUP_USER_DATA_GROUPS     , &XmlParser::OnStartElementFlibraryBackupUserDataItem },
			{ FLIBRARY_BACKUP_USER_DATA_SEARCHES   , &XmlParser::OnStartElementFlibraryBackupUserDataItem },
			{ FLIBRARY_BACKUP_USER_DATA_EXPORT_STAT, &XmlParser::OnStartElementFlibraryBackupUserDataItem },
		};

		const auto result = Parse(*this, PARSERS, path, name, attributes);
		if (!IsLastItemProcessed())
			m_restorers.back()->AddElement(name, attributes);

		return result;
	}

	bool OnEndElement(const QString & /*name*/, const QString & /*path*/) override
	{
		return true;
	}

	bool OnCharacters(const QString & /*path*/, const QString & /*value*/) override
	{
		return true;
	}

	bool OnWarning(const QString & text) override
	{
		PLOGW << text;
		return true;
	}

	bool OnError(const QString & text) override
	{
		PLOGE << text;
		return true;
	}

	bool OnFatalError(const QString & text) override
	{
		return OnError(text);
	}

private:
	bool OnStartElementFlibraryBackup(const QString &, const Util::XmlAttributes &)
	{
		m_check |= Check::RootNodeFound;
		return true;
	}

	bool OnStartElementFlibraryBackupVersion(const QString &, const Util::XmlAttributes & attributes)
	{
		m_check |= Check::VersionNodeFound;

		const auto versionValue = attributes.GetAttribute(Constant::VALUE);
		if (versionValue.isEmpty())
			return false;
		m_check |= Check::VersionAttributeFound;

		bool ok = false;
		m_version = versionValue.toInt(&ok);
		if (!ok)
			return false;
		m_check |= Check::VersionAttributeMustBeInteger;

		if (m_version < 1 || m_version > Constant::FlibraryBackupVersionNumber)
			return false;
		m_check |= Check::VersionNumberMustBeActual;

		return true;
	}

	bool OnStartElementFlibraryBackupUserData(const QString &, const Util::XmlAttributes &)
	{
		m_check |= Check::UserDataNodeFound;
		Validate();
		return m_error.isEmpty();
	}

	bool OnStartElementFlibraryBackupUserDataItem(const QString & name, const Util::XmlAttributes &)
	{
		const auto it = m_restorerCreators.find(name);
		assert(it != m_restorerCreators.end());

		const auto itParser = it->second.upper_bound(m_version);
		assert(itParser != it->second.begin());
		m_restorers.push_back(std::prev(itParser)->second());

		return true;
	}

private:
	void Validate()
	{
		using ErrorGetter = std::function<QString()>;
		const ErrorGetter check_errors[]
		{
			[]{return Tr(QT_TRANSLATE_NOOP("UserData", "Invalid root node name, must be %1")).arg(Constant::FlibraryBackup);},
			[]{return Tr(QT_TRANSLATE_NOOP("UserData", "Cannot find version node, must be %1")).arg(Constant::FlibraryBackupVersion);},
			[]{return Tr(QT_TRANSLATE_NOOP("UserData", "Cannot find version attribute, must be %1")).arg(Constant::VALUE);},
			[]{return Tr(QT_TRANSLATE_NOOP("UserData", "%1: must be integer")).arg(Constant::VALUE);},
			[this]{return Tr(QT_TRANSLATE_NOOP("UserData", "Version %1 must be greater than 0 and less than %2")).arg(m_version).arg(Constant::FlibraryBackupVersionNumber + 1); },
			[]{return Tr(QT_TRANSLATE_NOOP("UserData", "Cannot find user data node, must be %1")).arg(Constant::FlibraryUserData);},
		};

		for (int i = 0, sz = static_cast<int>(std::size(check_errors)); i < sz; ++i)
		{
			if (!(m_check & static_cast<Check>(1 << i)))
			{
				m_error = check_errors[i]();
				return;
			}
		}
	}

private:
	std::vector<std::unique_ptr<IRestorer>> m_restorers;
	Check m_check { Check::None };
	using RestorerCreator = std::unique_ptr<IRestorer>(*)();
	std::unordered_map<QString, std::map<int, RestorerCreator>> m_restorerCreators;

	int m_version { -1 };
	QString m_error;
};

void RestoreImpl(const Util::IExecutor & executor, DB::IDatabase & db, QString fileName, Callback callback)
{
	executor({ "Restore user data", [&db, fileName = std::move(fileName), callback = std::move(callback)] () mutable
	{
		auto createResult = [callback = std::move(callback)] (QString error = {}) mutable
		{
			if (!error.isEmpty())
				PLOGE << error;

			return [callback = std::move(callback), error = std::move(error)] (size_t)
			{
				callback(error);
			};
		};

		QFile inp(fileName);
		if (!inp.open(QIODevice::ReadOnly))
			return createResult(QString(CANNOT_READ_FROM).arg(fileName));

		const XmlParser parser(inp, db);
		return createResult(parser.GetError());
	} });
}

}

void Restore(const Util::IExecutor & executor, DB::IDatabase & db, QString fileName, Callback callback)
{
	RestoreImpl(executor, db, std::move(fileName), std::move(callback));
}

}
