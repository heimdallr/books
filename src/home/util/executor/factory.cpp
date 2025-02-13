#include "factory.h"

#include <cassert>

#include "IExecutor.h"

#define UTIL_EXECUTOR_IMPL(NAME)                                    \
	namespace HomeCompa::Util::ExecutorPrivate::NAME                \
	{                                                               \
	std::unique_ptr<IExecutor> CreateExecutor(ExecutorInitializer); \
	}
UTIL_EXECUTOR_IMPLS_XMACRO
#undef UTIL_EXECUTOR_IMPL

namespace HomeCompa::Util::ExecutorFactory
{

namespace
{

using FactoryCreator = std::unique_ptr<IExecutor> (*)(ExecutorInitializer);
constexpr FactoryCreator g_creators[] {
#define UTIL_EXECUTOR_IMPL(NAME) &ExecutorPrivate::NAME::CreateExecutor,
	UTIL_EXECUTOR_IMPLS_XMACRO
#undef UTIL_EXECUTOR_IMPL
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

std::unique_ptr<IExecutor> Create(const ExecutorImpl impl, ExecutorInitializer initializer)
{
	const auto creator = GetCreator(impl);
	assert(creator);
	return creator(std::move(initializer));
}

} // namespace HomeCompa::Util::ExecutorFactory
