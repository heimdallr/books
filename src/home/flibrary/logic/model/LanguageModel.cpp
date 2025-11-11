#include "LanguageModel.h"

#include <ranges>

#include <QSortFilterProxyModel>

#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/localization.h"

#include "util/language.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Role = ILanguageModel::Role;

constexpr auto CONTEXT = "LanguageModel";

TR_DEF

class ModelImpl : public QAbstractTableModel
{
	struct Item
	{
		QString language;
		int     count { 0 };
		bool    checked { false };
	};

	using Items = std::vector<Item>;

public:
	explicit ModelImpl(std::shared_ptr<const IDatabaseUser> databaseUser)
	{
		const auto& databaseUserRef = *databaseUser;
		databaseUserRef.Execute({ "Create language list", [&, databaseUser = std::move(databaseUser)]() mutable {
									 const auto db    = databaseUser->Database();
									 const auto query = db->CreateQuery("select Lang, count(42) from Books group by Lang");
									 Items      items;
									 for (query->Execute(); !query->Eof(); query->Next())
										 items.emplace_back(query->Get<const char*>(0), query->Get<int>(1));

									 return [this, items = std::move(items)](size_t) mutable {
										 Reset(std::move(items));
									 };
								 } });
	}

private: // QAbstractItemModel
	QVariant headerData(const int section, const Qt::Orientation orientation, const int role) const override
	{
		static constexpr const char* headers[] {
			QT_TRANSLATE_NOOP("LanguageModel", "Code"),
			QT_TRANSLATE_NOOP("LanguageModel", "Count"),
			QT_TRANSLATE_NOOP("LanguageModel", "Language"),
		};

		return orientation == Qt::Orientation::Horizontal && role == Qt::DisplayRole ? Tr(headers[section]) : QVariant {};
	}

	int rowCount(const QModelIndex& parent) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(m_items.size());
	}

	int columnCount(const QModelIndex&) const override
	{
		return 3;
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		if (!index.isValid())
		{
			assert(role == Role::SelectedList);
			QStringList result;
			std::ranges::transform(
				m_items | std::views::filter([](const Item& item) {
					return item.checked;
				}),
				std::back_inserter(result),
				[](const Item& item) {
					return item.language;
				}
			);
			return result;
		}

		const auto& item = m_items[static_cast<size_t>(index.row())];

		switch (role)
		{
			case Qt::DisplayRole:
				switch (index.column())
				{
					case 0:
						return item.language;
					case 1:
						return item.count;
					case 2:
						if (const auto it = m_translations.find(item.language); it != m_translations.end())
							return Loc::Tr(LANGUAGES_CONTEXT, it->second);
						return {};
					default:
						assert(false && "unexpected column");
						return {};
				}

			case Qt::CheckStateRole:
				return index.column() != 0 ? QVariant {} : item.checked ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;

			default:
				break;
		}

		return {};
	}

	bool setData(const QModelIndex& index, const QVariant& value, [[maybe_unused]] const int role) override
	{
		if (index.isValid())
		{
			assert(role == Qt::CheckStateRole);
			auto& item   = m_items[static_cast<size_t>(index.row())];
			item.checked = value.value<Qt::CheckState>() == Qt::CheckState::Checked;
			return true;
		}

		switch (role)
		{
			case Role::CheckAll:
				return SetChecks([](const auto&) {
					return true;
				});
			case Role::UncheckAll:
				return SetChecks([](const auto&) {
					return false;
				});
			case Role::RevertChecks:
				return SetChecks([](const auto& item) {
					return !item.checked;
				});
			case Role::SelectedList:
				if (!Util::Set(m_checked, value.toStringList()))
					return false;
				return Reset(), true;

			default:
				break;
		}

		return assert(false && "unexpected role"), false;
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		auto flags = QAbstractTableModel::flags(index);
		return index.column() == 0 ? flags |= Qt::ItemIsUserCheckable : flags &= ~Qt::ItemIsUserCheckable;
	}

private:
	bool SetChecks(const std::function<bool(const Item&)>& f)
	{
		std::vector<int> indices;
		std::ranges::for_each(m_items, [&, n = 0](Item& item) mutable {
			if (Util::Set(item.checked, f(item)))
				indices.emplace_back(n);
			++n;
		});

		for (const auto [begin, end] : Util::CreateRanges(indices))
			emit dataChanged(index(begin, 0), index(end - 1, 0), { Qt::CheckStateRole });

		return !indices.empty();
	}

	void Reset(Items items = {})
	{
		const ScopedCall resetGuard(
			[this] {
				beginResetModel();
			},
			[this] {
				endResetModel();
			}
		);
		if (!items.empty())
			m_items = std::move(items);

		std::unordered_set languagesIndexed(std::make_move_iterator(m_checked.begin()), std::make_move_iterator(m_checked.end()));
		for (auto& item : m_items)
			item.checked = languagesIndexed.contains(item.language);
	}

private:
	Items                                          m_items;
	const std::unordered_map<QString, const char*> m_translations { GetLanguagesMap() };
	QStringList                                    m_checked;
};

} // namespace

class LanguageModel::Model final : public QSortFilterProxyModel
{
public:
	explicit Model(std::shared_ptr<const IDatabaseUser> databaseUser)
		: m_source { std::unique_ptr<QAbstractItemModel> { std::make_unique<ModelImpl>(std::move(databaseUser)) } }
	{
		QSortFilterProxyModel::setSourceModel(m_source.get());
	}

private:
	PropagateConstPtr<QAbstractItemModel> m_source;
};

LanguageModel::LanguageModel(std::shared_ptr<const IDatabaseUser> databaseUser)
	: m_model(std::move(databaseUser))
{
}

LanguageModel::~LanguageModel() = default;

QAbstractItemModel* LanguageModel::GetModel() noexcept
{
	return m_model.get();
}
