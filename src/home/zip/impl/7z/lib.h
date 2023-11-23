#pragma once

#include "fnd/memory.h"

struct _GUID;

namespace HomeCompa::Zip::Impl::SevenZip
{
class Lib
{
public:
	Lib();
	~Lib();

	bool CreateObject( const _GUID& clsID, const _GUID& interfaceID, void** outObject ) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};
}
