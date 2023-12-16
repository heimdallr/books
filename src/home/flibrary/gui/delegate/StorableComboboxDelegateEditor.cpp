#include "StorableComboboxDelegateEditor.h"

#include <ranges>

#include <QComboBox>

#include "fnd/memory.h"
#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto KEY_TEMPLATE = "%1/%2";
}

struct StorableComboboxDelegateEditor::Impl : QComboBox
{
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	QString settingsKey;
	std::map<int, QString> values;

	explicit Impl(std::shared_ptr<ISettings> settings)
		: settings(std::move(settings))
	{
		setEditable(true);
	}
};

StorableComboboxDelegateEditor::StorableComboboxDelegateEditor(std::shared_ptr<ISettings> settings
	, QWidget * parent
)
	: BaseDelegateEditor(this, parent)
	, m_impl(std::make_unique<Impl>(std::move(settings)))
{
}

StorableComboboxDelegateEditor::~StorableComboboxDelegateEditor() = default;

void StorableComboboxDelegateEditor::SetSettingsKey(QString key)
{
	m_impl->settingsKey = std::move(key);
	SettingsGroup group(*m_impl->settings, m_impl->settingsKey);
	std::vector<QString> values;
	std::ranges::transform(m_impl->settings->GetKeys(), std::back_inserter(values), [&] (const auto & number)
	{
		return m_impl->values.emplace(number.toInt(), m_impl->settings->Get(number).toString()).first->second;
	});
	std::ranges::sort(values);
	for (const auto & value : values)
		m_impl->addItem(value);
}

QWidget * StorableComboboxDelegateEditor::GetWidget() const noexcept
{
	return m_impl.get();
}

QString StorableComboboxDelegateEditor::GetText() const
{
	return m_impl->currentText();
}

void StorableComboboxDelegateEditor::SetText(const QString & value)
{
	m_impl->setEditText(value);
}

void StorableComboboxDelegateEditor::SetParent(QWidget * parent)
{
	m_impl->setParent(parent);
}

void StorableComboboxDelegateEditor::OnSetModelData(const QString & value)
{
	if (std::ranges::any_of(m_impl->values | std::views::values, [&](const auto & item){ return !item.compare(value, Qt::CaseInsensitive); }))
		return;

	const auto id = m_impl->values.empty() ? 0 : std::prev(m_impl->values.cend())->first + 1;
	m_impl->settings->Set(QString(KEY_TEMPLATE).arg(m_impl->settingsKey).arg(id), value);
}
