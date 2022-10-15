#pragma once

namespace HomeCompa::Util {

template
	< typename T
	, typename Obj
	, typename Signal = void(Obj:: *)()
	>
static bool Set(T & dst, const T & value, Obj & obj, const Signal signal)
{
	if (dst == value)
		return false;

	dst = value;
	emit (obj.*signal)();
	return true;
}

template
	< typename T
	, typename Obj
	, typename Signal = void(Obj:: *)() const
	>
static bool Set(T & dst, const T & value, const Obj & obj, const Signal signal)
{
	if (dst == value)
		return false;

	dst = value;
	emit (obj.*signal)();
	return true;
}

}
