#pragma once

#include <unordered_map>
#include <unordered_set>

#include "private/ObserverHelpers.h"

namespace HomeCompa {

template<typename T>
class Observable
	: public ObserverHelper::Observable
{
public:
	~Observable() override
	{
		for (auto * observer : m_observers)
			observer->Unregister(this);
	}

	void Register(T * observer)
	{
		observer->Register(this);
		[[maybe_unused]] auto inserted = m_observers.emplace(observer).second;
		inserted = m_observersMap.emplace(observer, observer).second;
		assert(inserted);
	}

	void Unregister(T * observer)
	{
		observer->Unregister(this);
		[[maybe_unused]] const auto result = Remove(observer);
		assert(result);
	}

	template <typename F, typename... ARGS>
	void Perform(F function, ARGS &&... args)
	{
		for (auto * const observer : m_observers)
			std::invoke(function, observer, std::forward<ARGS>(args)...);
	}

private: // ObserverHelper::Observable
	void HandleObserverDestructed(Observer * observer) override
	{
		Remove(observer);
	}

private:
	bool Remove(Observer * observer)
	{
		const auto it = m_observersMap.find(observer);
		if (it == m_observersMap.end())
			return false;

		const auto result = m_observers.erase(it->second) == 1;
		m_observersMap.erase(it);

		return true;
	}

private:
	std::unordered_set<T *> m_observers;
	std::unordered_map<Observer *, T *> m_observersMap;
};

}
