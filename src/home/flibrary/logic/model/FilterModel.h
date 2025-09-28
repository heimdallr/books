#pragma once

#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"

#include "interface/constants/ModelRole.h"
#include "interface/logic/IFilterController.h"
#include "interface/logic/IModelProvider.h"

#include "util/localization.h"

namespace HomeCompa::Flibrary
{

constexpr auto CONTEXT = "FilterModel";
constexpr const char* HEADERS[] {
	QT_TRANSLATE_NOOP("FilterModel", "Title"),
	QT_TRANSLATE_NOOP("FilterModel", "Hidden"),
	QT_TRANSLATE_NOOP("FilterModel", "Books filtered"),
};
TR_DEF

template <typename ModelType>
class FilterModel : public ModelType
{
	using BaseType = ModelType;

protected:
	FilterModel(const std::shared_ptr<IModelProvider>& modelProvider, std::shared_ptr<IFilterController> filterController, QObject* parent = nullptr)
		: ModelType(modelProvider, parent)
		, filterController { std::move(filterController) }
	{
	}

protected: // QAbstractItemModel
	QVariant headerData(const int section, const Qt::Orientation orientation, const int role) const override
	{
		return orientation == Qt::Horizontal && role == Qt::DisplayRole ? Tr(HEADERS[section]) : BaseType::headerData(section, orientation, role);
	}

	int columnCount(const QModelIndex& /*parent*/) const override
	{
		return static_cast<int>(std::size(HEADERS));
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		if (!index.isValid())
			return BaseType::data(index, role);

		const auto* item = this->GetInternalPointer(index);
		switch (role)
		{
			case Qt::DisplayRole:
			case Qt::ToolTipRole:
				return index.column() == 0 ? item->GetData() : QVariant {};

			case Qt::CheckStateRole:
			case Role::CheckState:
			{
				if (index.column() == 0)
					return {};

				const auto flags = index.column() == 1 ? IDataItem::Flags::Filtered : index.column() == 2 ? IDataItem::Flags::BooksFiltered : IDataItem::Flags::None;
				return IsChecked(*item, flags);
			}

			default:
				break;
		}

		return BaseType::data(index, role);
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		if (!index.isValid())
		{
			switch (role)
			{
				case Role::NavigationMode:
					return Util::Set(navigationMode, value.value<NavigationMode>());

				case Role::HideFiltered:
					return filterController->HideFiltered(navigationMode, this, value.value<IFilterController::ICallback*>()->Ptr()), true;

				case Role::HideFilteredCallback:
					return HideFilteredCallback(value);

				default:
					break;
			}
			return BaseType::setData(index, value, role);
		}

		auto item = this->GetInternalPointer(index);
		switch (role)
		{
			case Qt::CheckStateRole:
			{
				const auto checkState = value.value<Qt::CheckState>();
				const auto flag = index.column() == 1 ? IDataItem::Flags::Filtered : index.column() == 2 ? IDataItem::Flags::BooksFiltered : IDataItem::Flags::None;
				item->SetFlags(checkState == Qt::Checked ? item->GetFlags() | flag : item->GetFlags() & ~flag);
				filterController->SetFlags(navigationMode, item->GetId(), item->GetFlags());
				return true;
			}

			case Role::Flags:
				if (BaseType::setData(index, value, role))
					return filterController->SetFlags(navigationMode, item->GetId(), item->GetFlags()), true;
				return false;

			default:
				break;
		}

		return BaseType::setData(index, value, role);
	}

protected:
	virtual std::vector<IDataItem::Ptr> GetHideFilteredChanged(const std::unordered_set<QString>& ids) const = 0;

	virtual QVariant IsChecked(const IDataItem& item, const IDataItem::Flags flags) const
	{
		return !!(item.GetFlags() & flags) ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;
	}

private:
	bool HideFilteredCallback(const QVariant& value)
	{
		const auto changed = GetHideFilteredChanged(value.value<std::unordered_set<QString>>());
		if (changed.empty())
			return false;

		const ScopedCall resetGuard([&] { this->beginResetModel(); }, [&] { this->endResetModel(); });
		for (auto& item : changed)
		{
			item->SetFlags(item->GetFlags() | IDataItem::Flags::Filtered);
			filterController->SetFlags(navigationMode, item->GetId(), item->GetFlags());
		}

		return true;
	}

protected:
	PropagateConstPtr<IFilterController, std::shared_ptr> filterController;
	NavigationMode navigationMode { NavigationMode::Unknown };
};

} // namespace HomeCompa::Flibrary
