#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IAuthorAnnotationController.h"

namespace HomeCompa::Flibrary::Test {

class MockAuthorAnnotationController : public IAuthorAnnotationController
{
public: // IAuthorAnnotationController
	MOCK_METHOD(bool, IsReady, (), (const, noexcept, override));
	MOCK_METHOD(void, SetNavigationMode, (NavigationMode mode), (override));
	MOCK_METHOD(void, SetAuthor, (long long id, QString name), (override));
	MOCK_METHOD(bool, CheckAuthor, (const QString& name), (const, override));
	MOCK_METHOD(QString, GetInfo, (const QString& name), (const, override));
	MOCK_METHOD(std::vector<QByteArray>, GetImages, (const QString& name), (const override));
	MOCK_METHOD(void, RegisterObserver, (IObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (IObserver * observer), (override));
};

static_assert(!std::is_abstract_v<MockAuthorAnnotationController>, "MockAuthorAnnotationController is abstract!");

}
