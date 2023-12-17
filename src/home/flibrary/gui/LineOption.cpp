#include "LineOption.h"

#include <QLineEdit>
#include <QString>

#include "fnd/observable.h"
#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

struct LineOption::Impl : Observable<IObserver>
{
	QLineEdit * lineEdit { nullptr };
	QString key;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	std::vector<QMetaObject::Connection> connections;

	explicit Impl(std::shared_ptr<ISettings> settings)
		: settings(std::move(settings))
	{
	}
};

LineOption::LineOption(std::shared_ptr<ISettings> settings)
	: m_impl(std::move(settings))
{
}

LineOption::~LineOption()
{
	for (auto & connection : m_impl->connections)
		QObject::disconnect(connection);
}

void LineOption::SetLineEdit(QLineEdit * lineEdit) noexcept
{
	m_impl->lineEdit = lineEdit;
	m_impl->connections.push_back(QObject::connect(m_impl->lineEdit, &QLineEdit::editingFinished, m_impl->lineEdit, [&]
	{
		m_impl->lineEdit->setVisible(false);
		m_impl->settings->Set(m_impl->key, m_impl->lineEdit->text());
		m_impl->Perform(&IObserver::OnOptionEditingFinished, m_impl->lineEdit->text());
	}));
	m_impl->connections.push_back(QObject::connect(m_impl->lineEdit, &QLineEdit::textChanged, m_impl->lineEdit, [&](const QString & text)
	{
		m_impl->Perform(&IObserver::OnOptionEditingFinished, text);
	}));
}

void LineOption::SetSettingsKey(QString key, const QString & defaultValue) noexcept
{
	m_impl->key = std::move(key);
	const auto value = m_impl->settings->Get(m_impl->key, defaultValue);
	m_impl->lineEdit->setText(value.isEmpty() ? defaultValue : value);
	m_impl->lineEdit->setVisible(true);
	m_impl->lineEdit->setFocus();
}

void LineOption::Register(IObserver * observer)
{
	m_impl->Register(observer);
}

void LineOption::Unregister(IObserver * observer)
{
	m_impl->Unregister(observer);
}
