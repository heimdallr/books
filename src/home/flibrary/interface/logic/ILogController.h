#pragma once

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class ILogController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ILogController() = default;
	virtual QAbstractItemModel * GetModel() const = 0;
};

}
