#include "ui_OpdsDialog.h"

#include "OpdsDialog.h"

#include <QClipboard>
#include <QNetworkInterface>
#include <QToolTip>

#include "interface/constants/SettingsConstant.h"

#include "gutil/GeometryRestorable.h"
#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto LABEL_LINK_TEMPLATE = R"(<a href="%1">%2</a>)";

constexpr auto CONTEXT = "OpdsDialog";
constexpr auto ADDRESS_COPIED = QT_TRANSLATE_NOOP("OpdsDialog", "Address has been copied to the clipboard");
constexpr auto NO_NETWORK_INTERFACES_FOUND = QT_TRANSLATE_NOOP("OpdsDialog", "No network interfaces found");
constexpr auto ANY = QT_TRANSLATE_NOOP("OpdsDialog", "Any");
constexpr auto SITE = QT_TRANSLATE_NOOP("OpdsDialog", "Site Address");
constexpr auto OPDS = QT_TRANSLATE_NOOP("OpdsDialog", "OPDS Address");
constexpr auto WEB = QT_TRANSLATE_NOOP("OpdsDialog", "Web Address");
TR_DEF
}

struct OpdsDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, IOpdsController::IObserver
{
	Ui::OpdsDialog ui {};
	QWidget& self;
	std::shared_ptr<ISettings> settings;
	std::shared_ptr<IOpdsController> opdsController;

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

		connect(ui.spinBoxPort, &QSpinBox::valueChanged, &self, [this] { OnConnectionChanged(); });
		connect(ui.comboBoxHosts, &QComboBox::currentIndexChanged, &self, [this] { OnConnectionChanged(); });
		connect(ui.btnStop, &QAbstractButton::clicked, &self, [this] { this->opdsController->Stop(); });
		connect(ui.btnStart,
		        &QAbstractButton::clicked,
		        &self,
		        [this]
		        {
					this->settings->Set(Constant::Settings::OPDS_PORT_KEY, ui.spinBoxPort->value());
					this->settings->Set(Constant::Settings::OPDS_HOST_KEY, ui.comboBoxHosts->currentData());
					this->opdsController->Start();
				});

		connect(ui.checkBoxAddToSturtup, &QAbstractButton::toggled, &self, [this](const bool checked) { checked ? this->opdsController->AddToStartup() : this->opdsController->RemoveFromStartup(); });

		const auto actionSetup = [&self](QAction* action, QLineEdit* lineEdit)
		{
			connect(action,
			        &QAction::triggered,
			        &self,
			        [=]
			        {
						QApplication::clipboard()->setText(lineEdit->text());
						QToolTip::showText(QCursor::pos(), Tr(ADDRESS_COPIED));
					});
			lineEdit->addAction(action, QLineEdit::TrailingPosition);
		};
		actionSetup(ui.actionCopyOpds, ui.lineEditOpdsAddress);
		actionSetup(ui.actionCopyWeb, ui.lineEditWebAddress);
		actionSetup(ui.actionCopySite, ui.lineEditSiteAddress);

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
		ui.lineEditOpdsAddress->setEnabled(isRunning);
		ui.lineEditWebAddress->setEnabled(isRunning);
		ui.lineEditSiteAddress->setEnabled(isRunning);
		ui.labelOpds->setOpenExternalLinks(isRunning);
		ui.labelWeb->setOpenExternalLinks(isRunning);
		ui.labelSite->setOpenExternalLinks(isRunning);
		ui.labelOpds->setEnabled(isRunning);
		ui.labelWeb->setEnabled(isRunning);
		ui.labelSite->setEnabled(isRunning);

		ui.btnStart->setVisible(!isRunning);
		ui.btnStop->setVisible(isRunning);

		SelLabelLink(isRunning);
	}

private:
	void OnConnectionChanged() const
	{
		const auto host = ui.comboBoxHosts->currentIndex() ? ui.comboBoxHosts->currentText() : ui.comboBoxHosts->itemText(1);
		ui.lineEditSiteAddress->setText(QString("http://%1:%2/").arg(host).arg(ui.spinBoxPort->value()));
		ui.lineEditOpdsAddress->setText(QString("http://%1:%2/opds").arg(host).arg(ui.spinBoxPort->value()));
		ui.lineEditWebAddress->setText(QString("http://%1:%2/web").arg(host).arg(ui.spinBoxPort->value()));

		SelLabelLink(true);
	}

	void SelLabelLink(const bool isRunning) const
	{
		if (isRunning)
		{
			ui.labelSite->setText(QString(LABEL_LINK_TEMPLATE).arg(ui.lineEditSiteAddress->text()).arg(Tr(SITE)));
			ui.labelOpds->setText(QString(LABEL_LINK_TEMPLATE).arg(ui.lineEditOpdsAddress->text()).arg(Tr(OPDS)));
			ui.labelWeb->setText(QString(LABEL_LINK_TEMPLATE).arg(ui.lineEditWebAddress->text()).arg(Tr(WEB)));
		}
		else
		{
			ui.labelSite->setText(Tr(SITE));
			ui.labelOpds->setText(Tr(OPDS));
			ui.labelWeb->setText(Tr(WEB));
		}
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
