#pragma once

namespace HomeCompa
{

template <typename T>
class ConvertibleT
{
public:
	template <typename U>
	U* To() noexcept
	{
		return static_cast<T*>(this);
	}

	template <typename U>
	const U* To() const noexcept
	{
		return static_cast<const T*>(this);
	}
};

}
