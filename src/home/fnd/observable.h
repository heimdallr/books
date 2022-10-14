#pragma once

#include <unordered_set>

namespace HomeCompa {

template<typename T>
class Observable
{
public:
	void Register(T * observer)
	{
		m_observers.insert(observer);
	}

	void Unregister(T * observer)
	{
		m_observers.erase(observer);
	}

	template <typename F, typename... ARGS>
	void Perform(F function, ARGS &&... args)
	{
		for (auto * observer : m_observers)
			std::invoke(function, observer, std::forward<ARGS>(args)...);
	}

private:
	std::unordered_set<T *> m_observers;
};

}
