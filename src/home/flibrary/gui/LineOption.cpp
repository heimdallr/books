#include "LineOption.h"

#include <QLineEdit>
#include <QString>

#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

struct LineOption::Impl
{
	QLineEdit * lineEdit { nullptr };
	QString key;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	QMetaObject::Connection connection;

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
	QObject::disconnect(m_impl->connection);
}

void LineOption::SetLineEdit(QLineEdit * lineEdit) noexcept
{
	m_impl->lineEdit = lineEdit;
	m_impl->connection = QObject::connect(m_impl->lineEdit, &QLineEdit::editingFinished, m_impl->lineEdit, [&]
	{
		m_impl->lineEdit->setVisible(false);
		m_impl->settings->Set(m_impl->key, m_impl->lineEdit->text());
	});
}

void LineOption::SetSettingsKey(QString key, const QString & defaultValue) noexcept
{
	m_impl->key = std::move(key);
	const auto value = m_impl->settings->Get(m_impl->key, defaultValue);
	m_impl->lineEdit->setText(value.isEmpty() ? defaultValue : value);
	m_impl->lineEdit->setVisible(true);
	m_impl->lineEdit->setFocus();
}
