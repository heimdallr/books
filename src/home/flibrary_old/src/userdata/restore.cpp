#include <QFile>
#include <QString>
#include <QXmlStreamReader>

#include <plog/Log.h>

#include "util/IExecutor.h"

#include "constants/ProductConstant.h"

#include "restore.h"

namespace HomeCompa::Flibrary {

#define RESTORE_ITEMS_XMACRO    \
		RESTORE_ITEM(Books, 1)  \
		RESTORE_ITEM(Groups, 1) \

namespace UserData {
#define RESTORE_ITEM(NAME, VERSION) void Restore##NAME##VERSION(DB::IDatabase & db, QXmlStreamReader & reader);
		RESTORE_ITEMS_XMACRO
#undef	RESTORE_ITEM
}

namespace {

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

constexpr RestoreItem g_restoreItems[]
{
#define RESTORE_ITEM(NAME, VERSION) { #NAME, VERSION, &UserData::Restore##NAME##VERSION },
		RESTORE_ITEMS_XMACRO
#undef	RESTORE_ITEM
		{ "z", std::numeric_limits<int>::max(), nullptr }
};
static_assert(std::is_sorted(std::cbegin(g_restoreItems), std::cend(g_restoreItems)));

void RestoreImpl(DB::IDatabase & db, QXmlStreamReader & reader, const int version)
{
	while (!reader.atEnd() && !reader.hasError())
	{
		const auto token = reader.readNext();
		if (token != QXmlStreamReader::StartElement)
			continue;

		const auto name = reader.name().toString().toStdString();
		RestoreItem nodeItem { name, version, nullptr };
		auto it = std::ranges::find_if(g_restoreItems, [&] (const auto & item) { return nodeItem < item; });

		assert(it != std::cend(g_restoreItems));
		--it;
		it->functor(db, reader);
	}
}

bool FindElement(QXmlStreamReader & reader, std::string_view name)
{
	while (!reader.atEnd() && !reader.hasError())
	{
		const auto token = reader.readNext();
		if (token == QXmlStreamReader::StartElement && reader.name().compare(name.data()) == 0)
			return true;
	}

	return false;
}

}

void Restore(Util::IExecutor & executor, DB::IDatabase & db, QString fileName)
{
	executor({ "Restore user data", [&db, fileName = std::move(fileName)]
	{
		auto stub = [](size_t) {};

		QFile inp(fileName);
		if (!inp.open(QIODevice::ReadOnly))
		{
			PLOGE << "Cannot read from " << fileName;
			return stub;
		}

		QXmlStreamReader reader(&inp);
		if (!FindElement(reader, Constant::FlibraryBackup))
		{
			PLOGE << "invalid root node name, must be " << Constant::FlibraryBackup;
			return stub;
		}

		if (!FindElement(reader, Constant::FlibraryBackupVersion))
		{
			PLOGE << "cannot find version node, must be " << Constant::FlibraryBackupVersion;
			return stub;
		}

		const auto attributes = reader.attributes();
		if (!attributes.hasAttribute(Constant::VALUE))
		{
			PLOGE << "cannot find version attribute, must be " << Constant::VALUE;
			return stub;
		}

		bool ok = false;
		const auto version = attributes.value(Constant::VALUE).toInt(&ok);
		if (!ok)
		{
			PLOGE << "cannot cast version attribute to int: '" << attributes.value(Constant::VALUE) << "'";
			return stub;
		}

		if (!FindElement(reader, Constant::FlibraryUserData))
		{
			PLOGE << "cannot find user data node, must be " << Constant::FlibraryUserData;
			return stub;
		}

		RestoreImpl(db, reader, version);

		return stub;
	} });
}

}
