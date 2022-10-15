#pragma once

#include <cassert>
#include <unordered_set>

#include "private/ObserverHelpers.h"

namespace HomeCompa {

class Observer
{
	template<typename T> friend class Observable;

public:
	~Observer()
	{
		for (auto * const observable : m_observables)
			observable->HandleObserverDestructed(this);
	}

private:
	void Register(ObserverHelper::Observable * observable)
	{
		[[maybe_unused]] const auto [_, inserted] = m_observables.emplace(observable);
		assert(inserted);
	}

	void Unregister(ObserverHelper::Observable * observable)
	{
		[[maybe_unused]] const auto result = m_observables.erase(observable);
		assert(result == 1);
	}

private:
	std::unordered_set<ObserverHelper::Observable *> m_observables;
};

}
