#pragma once

#include <QHash>

#include <unordered_set>

#include <QString>

#include "export/logic.h"

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::DB
{
class IDatabase;
}

namespace HomeCompa::Flibrary
{

struct Update
{
	using CodeType = long long;
	CodeType code { 0 };
	QString name;
	size_t row { 0 };
	Update* parent { nullptr };
	std::vector<Update> children;
	bool removed { false };

	LOGIC_EXPORT static Update Load(DB::IDatabase& db, const std::unordered_set<long long>& neededUpdates = {});
	LOGIC_EXPORT static Update* Find(Update* root, long long code);

	static const Update* Find(const Update* root, const long long code)
	{
		return Find(const_cast<Update*>(root), code);
	}
};

struct Genre
{
	using CodeType = QString;
	QString fb2Code;
	CodeType code;
	QString name;
	bool checked { false };
	size_t row { 0 };
	Genre* parent { nullptr };
	std::vector<Genre> children;
	bool removed { false };

	LOGIC_EXPORT static Genre Load(DB::IDatabase& db, const std::unordered_set<QString>& neededGenres = {});
	LOGIC_EXPORT static Genre* Find(Genre* root, const QString& code);
	LOGIC_EXPORT static void SetSortMode(const ISettings& settings);

	static const Genre* Find(const Genre* root, const QString& code)
	{
		return Find(const_cast<Genre*>(root), code);
	}
};

} // namespace HomeCompa::Flibrary
