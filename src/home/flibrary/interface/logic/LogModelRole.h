#pragma once

#include <qnamespace.h>

namespace HomeCompa::Flibrary
{

struct LogModelRole
{
	enum Value
	{
		Message = Qt::UserRole + 1,
		Severity,
		Clear,
	};
};

}
