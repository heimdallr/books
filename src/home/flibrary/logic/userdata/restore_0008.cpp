#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IFilterProvider.h"

#include "constants/filter.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"

using namespace HomeCompa::Flibrary::Constant::UserData::Filter;
using namespace HomeCompa::Flibrary::UserData;
using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

class FilterRestorer final : virtual public IRestorer
{
	using DataValues = std::vector<std::pair<QString, int>>;

private: // IRestorer
	void AddElement(const QString& name, const Util::XmlAttributes& attributes) override
	{
		if (name != Constant::ITEM)
			return (void)(m_dataValues = &m_data[name]);

		assert(m_dataValues);
		m_dataValues->emplace_back(attributes.GetAttribute(Title), attributes.GetAttribute(Flag).toInt());
	}

	void Restore(DB::IDatabase& db) const override
	{
		const auto tr    = db.CreateTransaction();
		auto       range = IFilterProvider::GetDescriptions();
		(void)std::ranges::for_each(range, [&](const IFilterProvider::FilteredNavigation& description) {
			const auto it = m_data.find(description.navigationTitle);
			if (it == m_data.end())
				return;

			const auto index = static_cast<size_t>(description.navigationMode);
			assert(!FIELD_NAMES[index].empty());
			tr->CreateCommand(std::format("update {} set Flags = 0", description.table))->Execute();
			const auto command = tr->CreateCommand(std::format("update {} set Flags = ? where {} = ?", description.table, FIELD_NAMES[index]));
			for (const auto& [title, flags] : it->second)
			{
				command->Bind(0, flags);
				command->Bind(1, title.toStdString());
				command->Execute();
			}
		});
		tr->Commit();
	}

private:
	DataValues*                             m_dataValues { nullptr };
	std::unordered_map<QString, DataValues> m_data;
};

} // namespace

namespace HomeCompa::Flibrary::UserData
{
std::unique_ptr<IRestorer> CreateFilterRestorer8()
{
	return std::make_unique<FilterRestorer>();
}
}
