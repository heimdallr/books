#pragma once

namespace HomeCompa
{
class Observer;
}

namespace HomeCompa::ObserverHelper
{

class IObservable
{
public:
	virtual ~IObservable()                                    = default;
	virtual void HandleObserverDestructed(Observer* observer) = 0;
};

}
