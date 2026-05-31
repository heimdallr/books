#include "ui_ModeLineEdit.h"

#include "ModeLineEdit.h"

#include <ranges>

#include <QMenu>

#include "settings/ISettings.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto TYPE = "type";

}

struct ModeLineEdit::Impl
{
	Ui::ModeLineEdit ui;

	PropagateConstPtr<ISettings, std::shared_ptr> settings { std::shared_ptr<ISettings> {} };
	QString                                       settingsKey;

	std::vector<std::pair<QAction*, IValueApplier::ValueApplier>> valueModeActions;
};

ModeLineEdit::ModeLineEdit(QWidget* parent)
	: QLineEdit(parent)
{
	auto& impl = *m_impl;
	impl.ui.setupUi(this);

	impl.valueModeActions.emplace_back(impl.ui.actionFindMode, &IValueApplier::Find);
	impl.valueModeActions.emplace_back(impl.ui.actionFilterMode, &IValueApplier::Filter);
	for (auto* action : impl.valueModeActions | std::views::keys)
	{
		addAction(action, LeadingPosition);
		action->setVisible(false);
	}

	for (const auto* action : impl.valueModeActions | std::views::keys)
		connect(action, &QAction::triggered, this, &ModeLineEdit::OnValueModeActionTriggered);
}

ModeLineEdit::~ModeLineEdit() = default;

ModeLineEdit::IValueApplier::ValueApplier ModeLineEdit::Setup(std::shared_ptr<ISettings> settings, QString settingsKey, const bool isFindDefault)
{
	IValueApplier::ValueApplier result = nullptr;

	auto& impl = *m_impl;
	impl.settings.reset(std::move(settings));
	impl.settingsKey = std::move(settingsKey);

	if (const auto it = std::ranges::find(
			impl.valueModeActions,
			impl.settings->Get(impl.settingsKey, (isFindDefault ? impl.ui.actionFindMode : impl.ui.actionFilterMode)->property(TYPE).toString()),
			[](const auto& item) {
				return item.first->property(TYPE).toString();
			}
		);
	    it != impl.valueModeActions.end())
	{
		it->first->setVisible(true);
		emit ValueApplierChanged(result = it->second);
	}

	assert(result);
	return result;
}

void ModeLineEdit::OnValueModeActionTriggered()
{
	auto& impl = *m_impl;
	QMenu menu;
	for (auto* item : impl.valueModeActions | std::views::keys)
	{
		auto* action = menu.addAction(item->icon(), item->text());
		connect(action, &QAction::triggered, this, [this, &impl, item] {
			impl.settings->Set(impl.settingsKey, item->property(TYPE));
			for (const auto& [modeAction, applier] : impl.valueModeActions)
			{
				const bool isActive = modeAction == item;
				modeAction->setVisible(isActive);
				if (isActive)
					emit ValueApplierChanged(applier);
			}
		});
	}
	menu.setFont(parentWidget()->font());
	if (!menu.isEmpty())
		menu.exec(QCursor::pos());
}
