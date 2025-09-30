#include "LineOption.h"

#include <QLineEdit>
#include <QString>

#include "fnd/observable.h"

#include "util/ISettings.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

struct LineOption::Impl : Observable<IObserver>
{
	QLineEdit*                                    lineEdit { nullptr };
	QString                                       key;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	std::vector<QMetaObject::Connection>          connections;

	explicit Impl(std::shared_ptr<ISettings> settings)
		: settings(std::move(settings))
	{
	}
};

LineOption::LineOption(std::shared_ptr<ISettings> settings)
	: m_impl(std::move(settings))
{
	PLOGV << "LineOption created";
}

LineOption::~LineOption()
{
	for (auto& connection : m_impl->connections)
		QObject::disconnect(connection);

	PLOGV << "LineOption destroyed";
}

void LineOption::SetLineEdit(QLineEdit* lineEdit) noexcept
{
	assert(lineEdit);
	m_impl->lineEdit = lineEdit;
	m_impl->connections.push_back(QObject::connect(lineEdit, &QLineEdit::editingFinished, lineEdit, [this, lineEdit] {
		lineEdit->setVisible(false);
		m_impl->settings->Set(m_impl->key, m_impl->lineEdit->text());
		m_impl->Perform(&IObserver::OnOptionEditingFinished, m_impl->lineEdit->text());
	}));
	m_impl->connections.push_back(QObject::connect(lineEdit, &QLineEdit::textChanged, lineEdit, [&](const QString& text) {
		m_impl->Perform(&IObserver::OnOptionEditing, text);
	}));
}

void LineOption::SetSettingsKey(QString key, const QString& defaultValue) noexcept
{
	m_impl->key      = std::move(key);
	const auto value = [&] {
		auto result = m_impl->settings->Get(m_impl->key, defaultValue);
		return result.isEmpty() ? defaultValue : result;
	}();
	auto&      lineEdit    = *m_impl->lineEdit;
	const bool needPerform = value == lineEdit.text();
	lineEdit.setText(value);
	lineEdit.setVisible(true);
	lineEdit.setFocus();
	if (needPerform)
		m_impl->Perform(&IObserver::OnOptionEditing, value);
}

void LineOption::Register(IObserver* observer)
{
	m_impl->Register(observer);
}

void LineOption::Unregister(IObserver* observer)
{
	m_impl->Unregister(observer);
}
