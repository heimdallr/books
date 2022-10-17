#pragma once

#include <functional>

#include <QObject>

#include "UtilLib.h"

namespace HomeCompa::Util {

class UTIL_API FunctorExecutionForwarder
	: public QObject
{
	Q_OBJECT

public:
	using FunctorType = std::function<void ()>;
	FunctorExecutionForwarder();
	void Forward(FunctorType f) const;

signals:
	void ExecuteFunctor(FunctorType f) const;

private slots:
	void OnExecuteFunctor(FunctorType f) const;
};

}
