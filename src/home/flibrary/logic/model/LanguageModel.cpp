#include "LanguageModel.h"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/logic/IDatabaseUser.h"

#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT = "LanguageModel";

TR_DEF

class ModelImpl : public QAbstractTableModel
{
	struct Item
	{
		QString language;
		int count;
		bool checked{ false };
	};
	using Items = std::vector<Item>;

public:
	explicit ModelImpl(std::shared_ptr<const IDatabaseUser> databaseUser)
	{
		const auto& databaseUserRef = *databaseUser;
		databaseUserRef.Execute({ "Create language list", [&, databaseUser = std::move(databaseUser)]() mutable
		{
			const auto db = databaseUser->Database();
			const auto query = db->CreateQuery("select Lang, count(42) from Books group by Lang");
			Items items;
			for (query->Execute(); !query->Eof(); query->Next())
				items.emplace_back(query->Get<const char*>(0), query->Get<int>(1));

			return [this, items = std::move(items)](size_t) mutable
			{
				beginResetModel();
				m_items = std::move(items);
				endResetModel();
			};
		} });
	}

private: // QAbstractItemModel
	QVariant headerData(const int section, const Qt::Orientation orientation, const int role) const override
	{
		static constexpr const char* headers[]
		{
			QT_TRANSLATE_NOOP("LanguageModel", "Code"),
			QT_TRANSLATE_NOOP("LanguageModel", "Count"),
			QT_TRANSLATE_NOOP("LanguageModel", "Language"),
		};

		return orientation == Qt::Orientation::Horizontal && role == Qt::DisplayRole
			? Tr(headers[section])
			: QVariant{};
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
		assert(index.isValid());
		const auto& item = m_items[index.row()];

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
			}

		case Qt::CheckStateRole:
			return index.column() != 0 ? QVariant{} : item.checked ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;

		default:
			break;
		}

		return {};
	}

	bool setData(const QModelIndex& index, const QVariant& value, [[maybe_unused]] const int role) override
	{
		assert(index.isValid() && role == Qt::CheckStateRole);
		auto& item = m_items[index.row()];
		item.checked = value.value<Qt::CheckState>() == Qt::CheckState::Checked;
		return true;
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		auto flags = QAbstractTableModel::flags(index);
		return index.column() == 0 ? flags |= Qt::ItemIsUserCheckable : flags &= ~Qt::ItemIsUserCheckable;
	}

private:
	Items m_items;
	std::unordered_map<QString, const char*> m_translations {std::cbegin(LANGUAGES), std::cend(LANGUAGES)};
};

}

class LanguageModel::Model final : public QSortFilterProxyModel
{
public:
	explicit Model(std::shared_ptr<const IDatabaseUser> databaseUser)
		: m_source{ std::unique_ptr<QAbstractItemModel>{std::make_unique<ModelImpl>(std::move(databaseUser))} }
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
