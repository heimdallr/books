// Common/MyTypes.h

#ifndef ZIP7_INC_COMMON_MY_TYPES_H
#define ZIP7_INC_COMMON_MY_TYPES_H

#include "../../C/7zTypes.h"

#include "Common0.h"

// typedef int HRes;
// typedef HRESULT HRes;

struct CBoolPair
{
	bool Val;
	bool Def;

	CBoolPair()
		: Val(false)
		, Def(false)
	{
	}

	void Init()
	{
		Val = false;
		Def = false;
	}

	void SetTrueTrue()
	{
		Val = true;
		Def = true;
	}

	void SetVal_as_Defined(bool val)
	{
		Val = val;
		Def = true;
	}
};

#endif
