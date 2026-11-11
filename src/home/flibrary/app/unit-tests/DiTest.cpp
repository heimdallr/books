#include <gtest/gtest.h>

#include "interface/logic/ILogicFactory.h"
#include "interface/ui/IUiFactory.h"

#include "Hypodermic/Hypodermic.h"
#include "src/home/flibrary/app/di_app.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace {

struct DiTest : testing::Test
{
	std::shared_ptr<Hypodermic::Container> container;

	void SetUp() override
	{
		Hypodermic::ContainerBuilder builder;
		DiInit(builder, container);
	}

	void TearDown() override
	{
		container.reset();
	}
};

TEST_F(DiTest, Create)
{
	std::ignore = container->resolve<ILogicFactory>();
	std::ignore = container->resolve<Util::IUiFactory>();
	std::ignore = container->resolve<IUiFactory>();
}

} // namespace
