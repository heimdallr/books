#pragma once

#include <gmock/gmock.h>

#include "gutil/interface/IParentWidgetProvider.h"

namespace HomeCompa::Util::Test {

class MockParentWidgetProvider : public IParentWidgetProvider
{
public: // IParentWidgetProvider
	MOCK_METHOD(void     , SetWidget, (QWidget* widget),(override));
	MOCK_METHOD(QWidget* , GetWidget, (QWidget* parentWidget), (const, override));
};

static_assert(!std::is_abstract_v<MockParentWidgetProvider>, "MockParentWidgetProvider is abstract!");

}
