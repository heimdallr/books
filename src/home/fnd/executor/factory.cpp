#include <cassert>

#include "executor.h"
#include "factory.h"

namespace HomeCompa::ExecutorPrivate::Sync {
std::unique_ptr<Executor> CreateExecutor();
}

namespace HomeCompa::ExecutorFactory {

namespace {

using FactoryCreator = std::unique_ptr<Executor>(*)();
constexpr FactoryCreator g_creators[]
{
	&ExecutorPrivate::Sync::CreateExecutor,
};

size_t ToIndex(const ExecutorImpl impl)
{
	const auto index = static_cast<size_t>(impl);
	assert(index < std::size(g_creators));
	return index;
}

auto GetCreator(const ExecutorImpl impl)
{
	return g_creators[ToIndex(impl)];
}

}

std::unique_ptr<Executor> Create(const ExecutorImpl impl)
{
	const auto creator = GetCreator(impl);
	assert(creator);
	return creator();
}

}
