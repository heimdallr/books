#pragma once

#include <memory>

namespace HomeCompa {

template<typename T>
class Lockable
{
public:
	static std::shared_ptr<const T> Lock(const std::weak_ptr<const T> & weak)
	{
		auto ptr = weak.lock();
		assert(ptr);
		return ptr;
	}
	static std::shared_ptr<T> Lock(const std::weak_ptr<T> & weak)
	{
		auto ptr = weak.lock();
		assert(ptr);
		return ptr;
	}
};

}
