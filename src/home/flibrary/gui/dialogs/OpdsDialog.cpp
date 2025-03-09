#include "ui_OpdsDialog.h"

#include "OpdsDialog.h"

#include <QClipboard>
#include <QNetworkInterface>
#include <QToolTip>

#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IOpdsController.h"

#include "GuiUtil/GeometryRestorable.h"
#include "GuiUtil/interface/IParentWidgetProvider.h"
#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "OpdsDialog";
constexpr auto ADDRESS_COPIED = QT_TRANSLATE_NOOP("OpdsDialog", "Address has been copied to the clipboard");
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

		ui.spinBoxPort->setValue(this->settings->Get(Constant::Settings::OPDS_PORT_KEY, Constant::Settings::OPDS_PORT_DEFAULT));
		ui.checkBoxAddToSturtup->setChecked(this->opdsController->InStartup());

		connect(ui.spinBoxPort, &QSpinBox::valueChanged, &self, [this] { OnPortChanged(); });
		connect(ui.btnStop, &QAbstractButton::clicked, &self, [this] { this->opdsController->Stop(); });
		connect(ui.btnStart,
		        &QAbstractButton::clicked,
		        &self,
		        [this]
		        {
					this->settings->Set(Constant::Settings::OPDS_PORT_KEY, ui.spinBoxPort->value());
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

		OnPortChanged();
		OnRunningChanged();
		Init();
	}

	~Impl() override
	{
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
		ui.spinBoxPort->setEnabled(!isRunning);
		ui.lineEditOpdsAddress->setEnabled(isRunning);
		ui.lineEditWebAddress->setEnabled(isRunning);
		ui.btnStart->setVisible(!isRunning);
		ui.btnStop->setVisible(isRunning);
	}

private:
	void OnPortChanged() const
	{
		const QHostAddress& localhost = QHostAddress(QHostAddress::LocalHost);
		for (const QHostAddress& address : QNetworkInterface::allAddresses())
		{
			if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost)
			{
				ui.lineEditOpdsAddress->setText(QString("http://%1:%2/opds").arg(address.toString()).arg(ui.spinBoxPort->value()));
				ui.lineEditWebAddress->setText(QString("http://%1:%2/web").arg(address.toString()).arg(ui.spinBoxPort->value()));
				return;
			}
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
