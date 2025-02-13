#pragma once

#include <QString>

class QAbstractItemModel;

namespace HomeCompa::Flibrary
{

class IModel // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	struct Role
	{
		enum
		{
			CheckAll = Qt::UserRole + 1,
			UncheckAll,
			RevertChecks,
			SelectedList,
		};
	};

public:
	virtual ~IModel() = default;
	virtual QAbstractItemModel* GetModel() noexcept = 0;
};

class ILanguageModel : virtual public IModel
{
};

class IGenreModel : virtual public IModel
{
};

} // namespace HomeCompa::Flibrary
