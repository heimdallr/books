#pragma once

class QString;

#include "constants/UserData.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Util {
class IExecutor;
class XmlAttributes;
}

namespace HomeCompa::Flibrary::UserData {

void Restore(Util::IExecutor & executor, DB::IDatabase & db, QString fileName, Callback callback);

class IRestorer  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IRestorer() = default;
	virtual void AddElement(const QString & name, const Util::XmlAttributes & attributes) = 0;
	virtual void Restore(DB::IDatabase & db) const = 0;
};

}
