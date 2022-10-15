#pragma once

namespace HomeCompa {
class Observer;
}

namespace HomeCompa::ObserverHelper {

class Observable
{
public:
	virtual ~Observable() = default;
	virtual void HandleObserverDestructed(Observer * observer) = 0;
};

}