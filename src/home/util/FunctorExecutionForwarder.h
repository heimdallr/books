#pragma once

#include <functional>

#include <QObject>

#include "util_export.h"

namespace HomeCompa::Util {

class UTIL_EXPORT FunctorExecutionForwarder
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
