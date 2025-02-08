#pragma once

#include <QString>

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class ILanguageModel // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ILanguageModel() = default;
	virtual QAbstractItemModel* GetModel() noexcept = 0;
};

}
