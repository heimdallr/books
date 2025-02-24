#include "UiFactory.h"

#include <QFileInfo>
#include <QPushButton>

#include <Hypodermic/Hypodermic.h>

#include "fnd/FindPair.h"

#include "interface/IParentWidgetProvider.h"

#include "util/ISettings.h"

#include "Dialog.h"
#include "log.h"

namespace HomeCompa::Util
{

namespace
{

constexpr auto RECENT_DIR_KEY = "ui/RecentDir/%1";

class RecentDir
{
public:
	RecentDir(std::shared_ptr<ISettings> settings, const QString& key)
		: m_settings(std::move(settings))
		, m_key(QString(RECENT_DIR_KEY).arg(key))
	{
	}

	QString GetDir(const QString& dir) const
	{
		return dir.isEmpty() ? m_settings->Get(m_key).toString() : dir;
	}

	void SetDir(QString name)
	{
		if (name.isEmpty())
			return;

		if (const QFileInfo fileInfo(name); fileInfo.isFile())
			name = fileInfo.dir().absolutePath();

		m_settings->Set(m_key, name);
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	const QString m_key;
};

} // namespace

struct UiFactory::Impl
{
	Hypodermic::Container& container;

	explicit Impl(Hypodermic::Container& container)
		: container(container)
	{
	}
};

UiFactory::UiFactory(Hypodermic::Container& container)
	: m_impl(container)
{
}

UiFactory::~UiFactory() = default;

QObject* UiFactory::GetParentObject(QObject* defaultObject) const noexcept
{
	if (auto* object = m_impl->container.resolve<IParentWidgetProvider>()->GetWidget())
		return object;
	return defaultObject;
}

QWidget* UiFactory::GetParentWidget(QWidget* defaultWidget) const noexcept
{
	if (auto* widget = m_impl->container.resolve<IParentWidgetProvider>()->GetWidget())
		return widget;
	return defaultWidget;
}

QMessageBox::ButtonRole UiFactory::ShowCustomDialog(const QMessageBox::Icon icon,
                                                    const QString& title,
                                                    const QString& text,
                                                    const std::vector<std::pair<QMessageBox::ButtonRole, QString>>& buttons,
                                                    const QMessageBox::ButtonRole defaultButton) const
{
	auto* parentWidget = m_impl->container.resolve<IParentWidgetProvider>()->GetWidget();
	m_impl->container.resolve<ISettings>();

	QMessageBox msgBox(parentWidget);
	msgBox.setFont(parentWidget->font());
	msgBox.setIcon(icon);
	msgBox.setWindowTitle(title);
	msgBox.setText(text);

	std::vector<std::pair<QAbstractButton*, QMessageBox::ButtonRole>> msgBoxButtons;
	msgBoxButtons.reserve(buttons.size());
	std::ranges::transform(buttons,
	                       std::back_inserter(msgBoxButtons),
	                       [&](const auto& item)
	                       {
							   auto* button = msgBox.addButton(item.second, item.first);
							   if (item.first == defaultButton)
								   msgBox.setDefaultButton(button);
							   return std::make_pair(button, item.first);
						   });

	msgBox.exec();

	return FindSecond(msgBoxButtons, msgBox.clickedButton(), QMessageBox::NoRole);
}

QMessageBox::StandardButton UiFactory::ShowQuestion(const QString& text, const QMessageBox::StandardButtons& buttons, const QMessageBox::StandardButton defaultButton) const
{
	const auto dialog = m_impl->container.resolve<IQuestionDialog>();
	return dialog->Show(text, buttons, defaultButton);
}

QMessageBox::StandardButton UiFactory::ShowWarning(const QString& text, const QMessageBox::StandardButtons& buttons, const QMessageBox::StandardButton defaultButton) const
{
	const auto dialog = m_impl->container.resolve<IWarningDialog>();
	return dialog->Show(text, buttons, defaultButton);
}

void UiFactory::ShowInfo(const QString& text) const
{
	const auto dialog = m_impl->container.resolve<IInfoDialog>();
	(void)dialog->Show(text);
}

void UiFactory::ShowError(const QString& text) const
{
	const auto dialog = m_impl->container.resolve<IErrorDialog>();
	(void)dialog->Show(text);
}

QString UiFactory::GetText(const QString& title, const QString& label, const QString& text, const QStringList& comboBoxItems, const QLineEdit::EchoMode mode) const
{
	const auto dialog = m_impl->container.resolve<IInputTextDialog>();
	return dialog->GetText(title, label, text, comboBoxItems, mode);
}

std::optional<QFont> UiFactory::GetFont(const QString& title, const QFont& font, const QFontDialog::FontDialogOptions& options) const
{
	bool ok = false;
	return QFontDialog::getFont(&ok, font, m_impl->container.resolve<IParentWidgetProvider>()->GetWidget(), title, options);
}

QString GetFileSystemObj(std::shared_ptr<ISettings> settings, const QString& key, const QString& dir, const std::function<QString(const QString&)>& f)
{
	RecentDir recentDir(std::move(settings), key);
	auto result = f(recentDir.GetDir(dir));
	recentDir.SetDir(result);
	return result;
}

QString UiFactory::GetOpenFileName(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const
{
	return GetFileSystemObj(m_impl->container.resolve<ISettings>(),
	                        key,
	                        dir,
	                        [&](const QString& recentDir)
	                        { return QFileDialog::getOpenFileName(m_impl->container.resolve<IParentWidgetProvider>()->GetWidget(), title, recentDir, filter, nullptr, options); });
}

QString UiFactory::GetSaveFileName(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const
{
	return GetFileSystemObj(m_impl->container.resolve<ISettings>(),
	                        key,
	                        dir,
	                        [&](const QString& recentDir)
	                        { return QFileDialog::getSaveFileName(m_impl->container.resolve<IParentWidgetProvider>()->GetWidget(), title, recentDir, filter, nullptr, options); });
}

QString UiFactory::GetExistingDirectory(const QString& key, const QString& title, const QString& dir, const QFileDialog::Options& options) const
{
	return GetFileSystemObj(m_impl->container.resolve<ISettings>(),
	                        key,
	                        dir,
	                        [&](const QString& recentDir) { return QFileDialog::getExistingDirectory(m_impl->container.resolve<IParentWidgetProvider>()->GetWidget(), title, recentDir, options); });
}

} // namespace HomeCompa::Util
