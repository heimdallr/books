#include "inpx/dictionary.h"

#include <gtest/gtest.h>

using namespace HomeCompa::Inpx;

namespace {

struct DictionaryTest : testing::Test
{
	static constexpr QStringView keyView   = u"testView";
	const QString                keyString = "keyString";

	Dictionary dictionary;
};

TEST_F(DictionaryTest, Empty)
{
	EXPECT_TRUE(dictionary.empty());
	EXPECT_EQ(dictionary.size(), 0);
}

TEST_F(DictionaryTest, Work)
{
	const auto [keyStringTest, valueStringTest] = dictionary.emplace(keyString, 42);
	EXPECT_EQ(keyString, keyString);
	EXPECT_EQ(valueStringTest, 42);

	const auto [keyStringViewTest, valueStringViewTest] = dictionary.emplace(keyView, 17);
	EXPECT_EQ(keyStringViewTest, keyView);
	EXPECT_EQ(valueStringViewTest, 17);

	auto it = dictionary.cbegin();
	EXPECT_EQ(it->first, keyString);
	EXPECT_EQ(it->second, 42);
	++it;
	EXPECT_EQ(it->first, keyView);
	EXPECT_EQ(it->second, 17);
	++it;
	EXPECT_EQ(it, dictionary.cend());

	EXPECT_FALSE(dictionary.empty());
	EXPECT_EQ(dictionary.size(), 2);

	EXPECT_EQ(dictionary.erase(QString { "test" }), 0);
	EXPECT_EQ(dictionary.erase(keyView), 1);
	EXPECT_EQ(dictionary.size(), 1);
	EXPECT_EQ(dictionary.erase(keyView), 0);
	EXPECT_EQ(dictionary.size(), 1);
	EXPECT_EQ(dictionary.erase(keyString), 1);
	EXPECT_EQ(dictionary.size(), 0);
	EXPECT_TRUE(dictionary.empty());
}

TEST_F(DictionaryTest, Find)
{
	EXPECT_FALSE(dictionary.find(keyString).has_value());
	EXPECT_FALSE(dictionary.find(keyView).has_value());

	dictionary.emplace(keyString, 42);
	dictionary.emplace(keyView, 17);
	EXPECT_FALSE(dictionary.empty());
	EXPECT_EQ(dictionary.size(), 2);

	EXPECT_FALSE(dictionary.find(QString { "test" }).has_value());

	const auto v42 = dictionary.find(keyString);
	EXPECT_TRUE(v42.has_value());
	EXPECT_EQ(*v42, 42);

	const auto v17 = dictionary.find(keyView);
	EXPECT_TRUE(v17.has_value());
	EXPECT_EQ(v17.value(), 17);
}

} // namespace
