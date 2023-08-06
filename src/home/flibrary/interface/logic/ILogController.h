#pragma once

#include <vector>

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class ILogController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ILogController() = default;
	virtual QAbstractItemModel * GetModel() const = 0;
	virtual void Clear() = 0;
	virtual std::vector<const char *> GetSeverities() const = 0;
	virtual int GetSeverity() const = 0;
	virtual void SetSeverity(int value) = 0;
};

}
