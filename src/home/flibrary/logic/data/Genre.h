#pragma once

#include <QString>

#include "export/logic.h"

namespace HomeCompa::DB
{
class IDatabase;
}

namespace HomeCompa::Flibrary
{

struct Genre
{
	QString code;
	QString name;
	bool checked { false };
	int row { 0 };
	Genre* parent { nullptr };
	std::vector<Genre> children;

	LOGIC_EXPORT static Genre Load(DB::IDatabase& db);
	LOGIC_EXPORT static Genre* Find(Genre* root, const QString& code);

	static const Genre* Find(const Genre* root, const QString& code)
	{
		return Find(const_cast<Genre*>(root), code);
	}
};

}
