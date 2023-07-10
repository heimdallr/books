#pragma once

#include "AppIntLib.h"

namespace HomeCompa {

class ILogicFactory
{
public:
	APPINT_API static const ILogicFactory & Instance();

public:
	class APPINT_API Guard
	{
	public:
		Guard(const ILogicFactory & logicFactory) noexcept;
		~Guard() noexcept;
	};

public:
	virtual ~ILogicFactory() = default;
};

}
