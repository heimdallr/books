#include "FunctorExecutionForwarder.h"

#include <QtCore>

namespace HomeCompa::Util
{

FunctorExecutionForwarder::FunctorExecutionForwarder()
{
	qRegisterMetaType<FunctorType>("FunctorType");

	[[maybe_unused]] const bool result = connect(this, &FunctorExecutionForwarder::ExecuteFunctor, this, &FunctorExecutionForwarder::OnExecuteFunctor, Qt::QueuedConnection);
	assert(result);
}

void FunctorExecutionForwarder::Forward(FunctorType f) const
{
	emit ExecuteFunctor(std::move(f));
}

void FunctorExecutionForwarder::OnExecuteFunctor(FunctorType f) const
{
	f();
}

}
