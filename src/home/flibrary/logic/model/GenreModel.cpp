#include "GenreModel.h"

#include <ranges>

#include <QAbstractItemModel>

#include "fnd/ScopedCall.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "inpx/src/util/constant.h"
#include "interface/constants/GenresLocalization.h"
#include "interface/logic/IDatabaseUser.h"
#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

struct Genre
{
	QString code;
	QString name;
	bool checked{ false };
	int row{ 0 };
	Genre* parent{ nullptr };
	std::vector<Genre> children;
};

template<typename T>
void Enumerate(T root, const auto& f)
{
	for (T child : root.children)
	{
		f(root, child);
		Enumerate<T>(child, f);
	}
}

Qt::CheckState GetChecked(const Genre& genre)
{
	if (genre.children.empty())
		return genre.checked ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;

	bool hasChecked = false, hasUnchecked = false;
	for (const auto& item : genre.children)
	{
		switch (GetChecked(item))
		{
		case Qt::CheckState::PartiallyChecked:
			return Qt::CheckState::PartiallyChecked;
		case Qt::CheckState::Checked:
			if (hasUnchecked)
				return Qt::CheckState::PartiallyChecked;
			hasChecked = true;
			break;
		case Qt::CheckState::Unchecked:
			if (hasChecked)
				return Qt::CheckState::PartiallyChecked;
			hasUnchecked = true;
			break;
		}
	}

	assert(!hasChecked || !hasUnchecked);
	return hasChecked ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;
}

}

class GenreModel::Model final : public QAbstractItemModel
{
public:
	explicit Model(std::shared_ptr<const IDatabaseUser> databaseUser)
	{
		CreateGenreTree(std::move(databaseUser));
	}

private: // QAbstractItemModel
	QModelIndex index(const int row, const int column, const QModelIndex& parent) const override
	{
		if (!hasIndex(row, column, parent))
			return {};

		auto* parentItem = parent.isValid() ? static_cast<Genre*>(parent.internalPointer()) : &m_root;
		auto& childItem = parentItem->children[row];

		return createIndex(row, column, &childItem);
	}

	QModelIndex parent(const QModelIndex& index) const override
	{
		if (!index.isValid())
			return {};

		auto* childItem = static_cast<Genre*>(index.internalPointer());
		auto* parentItem = childItem->parent;

		return parentItem != &m_root ? createIndex(parentItem->row, 0, parentItem) : QModelIndex{};
	}

	int rowCount(const QModelIndex& parent) const override
	{
		if (parent.column() > 0)
			return 0;

		const auto* parentItem = parent.isValid() ? static_cast<Genre*>(parent.internalPointer()) : &m_root;
		return static_cast<int>(parentItem->children.size());
	}

	int columnCount(const QModelIndex&) const override
	{
		return 1;
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		if (!index.isValid())
		{
			assert(role == Role::SelectedList);
			QStringList result;
			Enumerate<const Genre&>(m_root, [&](const Genre&, const Genre& item)
			{
				if (item.children.empty() && item.checked)
					result << item.code;
			});
			return result;
		}

		const auto* genre = static_cast<Genre*>(index.internalPointer());
		switch (role)
		{
			case Qt::DisplayRole:
				return genre->name;

			case Qt::CheckStateRole:
				return GetChecked(*static_cast<Genre*>(index.internalPointer()));

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
			SetChecked(index, value.value<Qt::CheckState>() == Qt::CheckState::Checked);
			return true;
		}

		switch (role)
		{
		case Role::CheckAll:
			return SetChecks([](const auto&) {return true; });
		case Role::UncheckAll:
			return SetChecks([](const auto&) {return false; });
		case Role::RevertChecks:
			return SetChecks([](const auto& item) {return !item.checked; });
		default:
			break;
		}

		return assert(false && "unexpected role"), false;
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsUserCheckable;
	}

private:
	bool SetChecks(const std::function<bool(const Genre&)>& f)
	{
		Enumerate<Genre&>(m_root, [&](Genre&, Genre& item)
		{
			if (item.children.empty())
				item.checked = f(item);
		});

		const auto update = [this](const QModelIndex& parent, const auto& updateRef) -> void
		{
			const auto sz = rowCount(parent);
			if (sz == 0)
				return;

			emit dataChanged(index(0, 0, parent), index(sz - 1, 0, parent), { Qt::CheckStateRole });
			for (int row = 0; row < sz; ++row)
				updateRef(index(row, 0, parent), updateRef);
		};
		update({}, update);

		return true;
	}

	void SetChecked(QModelIndex index, const bool value)
	{
		assert(index.isValid());

		const auto sz = rowCount(index);
		if (sz == 0)
		{
			auto& genre = *static_cast<Genre*>(index.internalPointer());
			genre.checked = value;

			for (; index.isValid(); index = index.parent())
				emit dataChanged(index, index, { Qt::CheckStateRole });
		}

		for (int row = 0; row < sz; ++row)
			SetChecked(this->index(row, 0, index), value);
	}

	void CreateGenreTree(std::shared_ptr<const IDatabaseUser> databaseUser)
	{
		const auto& databaseUserRef = *databaseUser;
		databaseUserRef.Execute({ "Create genre list", [&, databaseUser = std::move(databaseUser)]() mutable
		{
			using AllGenresItem = std::tuple<Genre, QString>;
			std::unordered_map<QString, AllGenresItem> allGenres;
			std::vector<AllGenresItem> buffer;
			const auto db = databaseUser->Database();
			const auto query = db->CreateQuery("select g.GenreCode, g.FB2Code, g.ParentCode, (select count(42) from Genre_List gl where gl.GenreCode = g.GenreCode) BookCount from Genres g");
			for (query->Execute(); !query->Eof(); query->Next())
			{
				AllGenresItem item{ Genre{ .code = query->Get<const char*>(0), .name = Loc::Tr(GENRE, query->Get<const char*>(1)) }, query->Get<const char*>(2) };
				if (query->Get<int>(3))
					buffer.emplace_back(std::move(item));
				else
					allGenres.try_emplace(query->Get<const char*>(0), std::move(item));
			}

			const auto updateChildren = [](auto& children)
			{
				std::ranges::sort(children, {}, [](const auto& item) { return item.code; });
				for (int i = 0, sz = static_cast<int>(children.size()); i < sz; ++i)
					children[i].row = i;
			};

			Genre root;
			while (!buffer.empty())
			{
				for (auto&& [genre, parentCode] : buffer)
				{
					updateChildren(genre.children);
					const auto it = allGenres.find(parentCode);
					(it == allGenres.end() ? root : std::get<0>(it->second)).children.emplace_back(std::move(genre));
				}
				buffer.clear();

				for (auto&& genre : allGenres | std::views::values | std::views::filter([](const auto& item) {return !get<0>(item).children.empty(); }))
					buffer.emplace_back(std::move(genre));
				for (const auto& [genre, _] : buffer)
					allGenres.erase(genre.code);
			}

			std::erase_if(root.children, [dateAddedCode = Loc::Tr(GENRE, QString::fromStdWString(DATE_ADDED_CODE).toStdString().data())](const Genre& item) { return item.name == dateAddedCode; });
			updateChildren(root.children);

			return [this, root = std::move(root)](size_t) mutable
			{
				const ScopedCall modelGuard([this] { beginResetModel(); }, [this] { endResetModel(); });
				m_root = std::move(root);
				Enumerate<Genre&>(m_root, [](Genre& parent, Genre& item) { item.parent = &parent; });
			};
		} });
	}

private:
	Genre m_root;
};

GenreModel::GenreModel(std::shared_ptr<const IDatabaseUser> databaseUser)
	: m_model(std::move(databaseUser))
{
}

GenreModel::~GenreModel() = default;

QAbstractItemModel* GenreModel::GetModel() noexcept
{
	return m_model.get();
}
