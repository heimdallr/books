#include <QFile>
#include <QString>
#include <QXmlStreamReader>

#include <plog/Log.h>

#include "util/IExecutor.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData {

#define RESTORE_ITEMS_X_MACRO     \
		RESTORE_ITEM(Books, 1)    \
		RESTORE_ITEM(Groups, 1)   \
		RESTORE_ITEM(Searches, 1) \

#define RESTORE_ITEM(NAME, VERSION) void Restore##NAME##VERSION(DB::IDatabase & db, QXmlStreamReader & reader);
		RESTORE_ITEMS_X_MACRO
#undef	RESTORE_ITEM

namespace {

constexpr auto CANNOT_READ_FROM = QT_TRANSLATE_NOOP("UserData", "Cannot read from %1");
constexpr auto INVALID_ROOT = QT_TRANSLATE_NOOP("UserData", "Invalid root node name, must be %1");
constexpr auto NO_VERSION_NODE = QT_TRANSLATE_NOOP("UserData", "Cannot find version node, must be %1");
constexpr auto NO_VERSION_ATTR = QT_TRANSLATE_NOOP("UserData", "Cannot find version attribute, must be %1");
constexpr auto INVALID_VERSION_ATTR = QT_TRANSLATE_NOOP("UserData", "%1: must be integer");
constexpr auto NO_USER_DATA_NODE = QT_TRANSLATE_NOOP("UserData", "Cannot find user data node, must be %1");

using RestoreFunctor = void(*)(DB::IDatabase & db, QXmlStreamReader & reader);
struct RestoreItem
{
	std::string_view node;
	int version;
	RestoreFunctor functor;
	constexpr bool operator<(const RestoreItem & rhs) const noexcept
	{
		return std::make_tuple(node, version) < std::make_tuple(rhs.node, rhs.version);
	}
};

constexpr RestoreItem RESTORE_ITEMS[]
{
#define RESTORE_ITEM(NAME, VERSION) { #NAME, VERSION, &UserData::Restore##NAME##VERSION },
		RESTORE_ITEMS_X_MACRO
#undef	RESTORE_ITEM
		{ "z", std::numeric_limits<int>::max(), nullptr }
};
static_assert(std::is_sorted(std::cbegin(RESTORE_ITEMS), std::cend(RESTORE_ITEMS)));

void RestoreImpl(DB::IDatabase & db, QXmlStreamReader & reader, const int version)
{
	while (!reader.atEnd() && !reader.hasError())
	{
		const auto token = reader.readNext();
		if (token != QXmlStreamReader::StartElement)
			continue;

		const auto name = reader.name().toString().toStdString();
		RestoreItem nodeItem { name, version, nullptr };
		auto it = std::ranges::find_if(RESTORE_ITEMS, [&] (const auto & item) { return nodeItem < item; });

		assert(it != std::cend(RESTORE_ITEMS));
		--it;
		it->functor(db, reader);
	}
}

bool FindElement(QXmlStreamReader & reader, const std::string_view name)
{
	while (!reader.atEnd() && !reader.hasError())
	{
		if (const auto token = reader.readNext(); token == QXmlStreamReader::StartElement && reader.name().compare(name.data()) == 0)
			return true;
	}

	return false;
}

}

void Restore(Util::IExecutor & executor, DB::IDatabase & db, QString fileName, Callback callback)
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

		QXmlStreamReader reader(&inp);
		if (!FindElement(reader, Constant::FlibraryBackup))
			return createResult(QString(INVALID_ROOT).arg(Constant::FlibraryBackup));

		if (!FindElement(reader, Constant::FlibraryBackupVersion))
			return createResult(QString(NO_VERSION_NODE).arg(Constant::FlibraryBackupVersion));

		const auto attributes = reader.attributes();
		if (!attributes.hasAttribute(Constant::VALUE))
			return createResult(QString(NO_VERSION_ATTR).arg(Constant::VALUE));

		bool ok = false;
		const auto version = attributes.value(Constant::VALUE).toInt(&ok);
		if (!ok)
			return createResult(QString(INVALID_VERSION_ATTR).arg(attributes.value(Constant::VALUE)));

		if (!FindElement(reader, Constant::FlibraryUserData))
			return createResult(QString(NO_USER_DATA_NODE).arg(Constant::FlibraryUserData));

		RestoreImpl(db, reader, version);

		return createResult();
	} });
}

}
