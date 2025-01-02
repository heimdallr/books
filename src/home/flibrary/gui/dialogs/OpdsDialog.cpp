#include "ui_OpdsDialog.h"
#include "OpdsDialog.h"

#include "GuiUtil/GeometryRestorable.h"
#include "GuiUtil/interface/IParentWidgetProvider.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IOpdsController.h"

using namespace HomeCompa;
using namespace Flibrary;

struct OpdsDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, IOpdsController::IObserver
{
	Ui::OpdsDialog ui {};
	QWidget & self;
	std::shared_ptr<ISettings> settings;
	std::shared_ptr<IOpdsController> opdsController;

	Impl(QDialog & self
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IOpdsController> opdsController
	)
		: GeometryRestorable(*this, settings, "OpdsDialog")
		, GeometryRestorableObserver(self)
		, self { self }
		, settings { std::move(settings) }
		, opdsController { std::move(opdsController) }
	{
		ui.setupUi(&self);
		this->opdsController->RegisterObserver(this);

		ui.spinBoxPort->setValue(this->settings->Get(Constant::Settings::OPDS_PORT_KEY, Constant::Settings::OPDS_PORT_DEFAULT).toInt());
		connect(ui.btnStop, &QAbstractButton::clicked, &self, [this] { this->opdsController->Stop(); });
		connect(ui.btnStart, &QAbstractButton::clicked, &self, [this]
		{
			this->settings->Set(Constant::Settings::OPDS_PORT_KEY, ui.spinBoxPort->value());
			this->opdsController->Start();
		});

		OnRunningChanged();
		Init();
	}

	~Impl() override
	{
		opdsController->UnregisterObserver(this);
	}

private: // GeometryRestorableObserver
	void OnFontChanged(const QFont &) override
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
		ui.btnStart->setEnabled(!isRunning);
		ui.btnStop->setEnabled(isRunning);
	}

	NON_COPY_MOVABLE(Impl);
};

OpdsDialog::OpdsDialog(const std::shared_ptr<IParentWidgetProvider> & parentWidgetProvider
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<IOpdsController> opdsController
	, QWidget *parent
)
	: QDialog(parent ? parent : parentWidgetProvider->GetWidget())
	, m_impl(*this
		, std::move(settings)
		, std::move(opdsController)
	)
{
}

OpdsDialog::~OpdsDialog() = default;
