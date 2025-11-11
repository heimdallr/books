#include "ui_OpdsDialog.h"

#include "OpdsDialog.h"

#include <QClipboard>
#include <QNetworkInterface>
#include <QToolTip>

#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"

#include "gutil/GeometryRestorable.h"
#include "util/hash.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto LABEL_LINK_TEMPLATE = R"(<a href="%1">%2</a>)";

constexpr auto CONTEXT                     = "OpdsDialog";
constexpr auto ADDRESS_COPIED              = QT_TRANSLATE_NOOP("OpdsDialog", "Address has been copied to the clipboard");
constexpr auto NO_NETWORK_INTERFACES_FOUND = QT_TRANSLATE_NOOP("OpdsDialog", "No network interfaces found");
constexpr auto ANY                         = QT_TRANSLATE_NOOP("OpdsDialog", "Any");
constexpr auto SITE                        = QT_TRANSLATE_NOOP("OpdsDialog", "ReactApp web interface");
constexpr auto OPDS                        = QT_TRANSLATE_NOOP("OpdsDialog", "OPDS");
constexpr auto WEB                         = QT_TRANSLATE_NOOP("OpdsDialog", "Simple web interface");
TR_DEF
}

struct OpdsDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, IOpdsController::IObserver
{
	Ui::OpdsDialog                                      ui {};
	QWidget&                                            self;
	PropagateConstPtr<ISettings, std::shared_ptr>       settings;
	PropagateConstPtr<IOpdsController, std::shared_ptr> opdsController;

	Impl(QDialog& self, std::shared_ptr<ISettings> settings, std::shared_ptr<IOpdsController> opdsController)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, self { self }
		, settings { std::move(settings) }
		, opdsController { std::move(opdsController) }
	{
		ui.setupUi(&self);
		this->opdsController->RegisterObserver(this);

		ui.comboBoxHosts->addItem(Tr(ANY), Constant::Settings::OPDS_HOST_DEFAULT);
		for (const QHostAddress& address : QNetworkInterface::allAddresses())
			if (address.protocol() == QAbstractSocket::IPv4Protocol)
				ui.comboBoxHosts->addItem(address.toString(), address.toString());

		if (const auto index = ui.comboBoxHosts->findData(this->settings->Get(Constant::Settings::OPDS_HOST_KEY, Constant::Settings::OPDS_HOST_DEFAULT)); index >= 0)
			ui.comboBoxHosts->setCurrentIndex(index);

		ui.spinBoxPort->setValue(this->settings->Get(Constant::Settings::OPDS_PORT_KEY, Constant::Settings::OPDS_PORT_DEFAULT));
		ui.checkBoxAddToSturtup->setChecked(this->opdsController->InStartup());
		ui.checkBoxAuth->setChecked(!this->settings->Get(Constant::Settings::OPDS_AUTH, QString {}).isEmpty());

		ui.checkBoxReactApp->setChecked(this->settings->Get(Constant::Settings::OPDS_REACT_APP_ENABLED, ui.checkBoxReactApp->isChecked()));
		ui.checkBoxSimpleWeb->setChecked(this->settings->Get(Constant::Settings::OPDS_WEB_ENABLED, ui.checkBoxSimpleWeb->isChecked()));
		ui.checkBoxOpds->setChecked(this->settings->Get(Constant::Settings::OPDS_OPDS_ENABLED, ui.checkBoxOpds->isChecked()));

		connect(ui.spinBoxPort, &QSpinBox::valueChanged, &self, [this] {
			OnConnectionChanged();
		});
		connect(ui.comboBoxHosts, &QComboBox::currentIndexChanged, &self, [this] {
			OnConnectionChanged();
		});
		connect(ui.btnStop, &QAbstractButton::clicked, &self, [this] {
			this->opdsController->Stop();
		});
		connect(ui.btnStart, &QAbstractButton::clicked, &self, [this] {
			this->settings->Set(Constant::Settings::OPDS_PORT_KEY, ui.spinBoxPort->value());
			this->settings->Set(Constant::Settings::OPDS_HOST_KEY, ui.comboBoxHosts->currentData());
			this->opdsController->Start();
		});

		connect(ui.checkBoxAddToSturtup, &QAbstractButton::toggled, &self, [this](const bool checked) {
			checked ? this->opdsController->AddToStartup() : this->opdsController->RemoveFromStartup();
		});
		connect(ui.lineEditOpdsUser, &QLineEdit::editingFinished, &self, [this] {
			SetAuth();
		});
		connect(ui.lineEditOpdsPassword, &QLineEdit::editingFinished, &self, [this] {
			SetAuth();
		});
		connect(ui.checkBoxAuth, &QAbstractButton::toggled, &self, [this](const bool checked) {
			if (!checked)
				this->settings->Remove(Constant::Settings::OPDS_AUTH);
		});

		const auto setChecked = [this](const QString& key, const bool isChecked) {
			this->settings->Set(key, isChecked);
		};

		connect(ui.checkBoxReactApp, &QAbstractButton::toggled, &self, [=](const bool isChecked) {
			setChecked(Constant::Settings::OPDS_REACT_APP_ENABLED, isChecked);
		});
		connect(ui.checkBoxSimpleWeb, &QAbstractButton::toggled, &self, [=](const bool isChecked) {
			setChecked(Constant::Settings::OPDS_WEB_ENABLED, isChecked);
		});
		connect(ui.checkBoxOpds, &QAbstractButton::toggled, &self, [=](const bool isChecked) {
			setChecked(Constant::Settings::OPDS_OPDS_ENABLED, isChecked);
		});

		const auto actionSetup = [&self](QAction* action, QLineEdit* lineEdit) {
			connect(action, &QAction::triggered, &self, [=] {
				QApplication::clipboard()->setText(lineEdit->text());
				QToolTip::showText(QCursor::pos(), Tr(ADDRESS_COPIED));
			});
			lineEdit->addAction(action, QLineEdit::TrailingPosition);
		};
		actionSetup(ui.actionCopyOpds, ui.lineEditOpds);
		actionSetup(ui.actionCopyWeb, ui.lineEditSimpleWeb);
		actionSetup(ui.actionCopySite, ui.lineEditReactApp);

		OnConnectionChanged();
		OnRunningChanged();
		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
		opdsController->UnregisterObserver(this);
	}

private: // GeometryRestorableObserver
	void OnFontChanged(const QFont&) override
	{
		self.adjustSize();
		const auto height = self.sizeHint().height();
		if (height < 0)
			return;

		self.setMinimumHeight(height);
		self.setMaximumHeight(height);
	}

private: // IOpdsController::IObserver
	void OnRunningChanged() override
	{
		const auto isRunning = opdsController->IsRunning();
		ui.comboBoxHosts->setEnabled(!isRunning);
		ui.spinBoxPort->setEnabled(!isRunning);

		ui.checkBoxSimpleWeb->setEnabled(!isRunning);
		ui.checkBoxReactApp->setEnabled(!isRunning);
		ui.checkBoxOpds->setEnabled(!isRunning);

		ui.lineEditSimpleWeb->setEnabled(isRunning && ui.checkBoxSimpleWeb->isChecked());
		ui.lineEditReactApp->setEnabled(isRunning && ui.checkBoxReactApp->isChecked());
		ui.lineEditOpds->setEnabled(isRunning && ui.checkBoxOpds->isChecked());

		ui.labelWeb->setOpenExternalLinks(isRunning && ui.checkBoxSimpleWeb->isChecked());
		ui.labelSite->setOpenExternalLinks(isRunning && ui.checkBoxReactApp->isChecked());
		ui.labelOpds->setOpenExternalLinks(isRunning && ui.checkBoxOpds->isChecked());

		ui.labelWeb->setEnabled(isRunning && ui.checkBoxSimpleWeb->isChecked());
		ui.labelSite->setEnabled(isRunning && ui.checkBoxReactApp->isChecked());
		ui.labelOpds->setEnabled(isRunning && ui.checkBoxOpds->isChecked());

		ui.btnStart->setVisible(!isRunning);
		ui.btnStop->setVisible(isRunning);

		ui.checkBoxAuth->setEnabled(!isRunning);
		ui.auth->setEnabled(!isRunning && ui.checkBoxAuth->isChecked());

		SelLabelLink(isRunning);
	}

private:
	void OnConnectionChanged() const
	{
		const auto host = ui.comboBoxHosts->currentIndex() ? ui.comboBoxHosts->currentText() : ui.comboBoxHosts->itemText(1);
		ui.lineEditReactApp->setText(QString("http://%1:%2/").arg(host).arg(ui.spinBoxPort->value()));
		ui.lineEditOpds->setText(QString("http://%1:%2/opds").arg(host).arg(ui.spinBoxPort->value()));
		ui.lineEditSimpleWeb->setText(QString("http://%1:%2/web").arg(host).arg(ui.spinBoxPort->value()));

		SelLabelLink(true);
	}

	void SelLabelLink(const bool isRunning) const
	{
		ui.labelSite->setText(isRunning && ui.checkBoxReactApp->isChecked() ? QString(LABEL_LINK_TEMPLATE).arg(ui.lineEditReactApp->text()).arg(Tr(SITE)) : Tr(SITE));
		ui.labelOpds->setText(isRunning && ui.checkBoxOpds->isChecked() ? QString(LABEL_LINK_TEMPLATE).arg(ui.lineEditOpds->text()).arg(Tr(OPDS)) : Tr(OPDS));
		ui.labelWeb->setText(isRunning && ui.checkBoxSimpleWeb->isChecked() ? QString(LABEL_LINK_TEMPLATE).arg(ui.lineEditSimpleWeb->text()).arg(Tr(WEB)) : Tr(WEB));
	}

	void SetAuth()
	{
		ui.lineEditOpdsUser->text().isEmpty() ? settings->Remove(Constant::Settings::OPDS_AUTH)
											  : settings->Set(Constant::Settings::OPDS_AUTH, Util::GetSaltedHash(ui.lineEditOpdsUser->text(), ui.lineEditOpdsPassword->text()));
	}

private:
	NON_COPY_MOVABLE(Impl)
};

OpdsDialog::OpdsDialog(const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider, std::shared_ptr<ISettings> settings, std::shared_ptr<IOpdsController> opdsController, QWidget* parent)
	: QDialog(parent ? parent : parentWidgetProvider->GetWidget())
	, m_impl(*this, std::move(settings), std::move(opdsController))
{
}

OpdsDialog::~OpdsDialog() = default;

int OpdsDialog::exec()
{
	if (m_impl->ui.comboBoxHosts->count() < 2)
		throw std::runtime_error(Tr(NO_NETWORK_INTERFACES_FOUND).toStdString());

	return QDialog::exec();
}
