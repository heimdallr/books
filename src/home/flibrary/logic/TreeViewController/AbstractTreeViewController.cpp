#include "AbstractTreeViewController.h"

#include <QModelIndex>

#include "interface/constants/SettingsConstant.h"

using namespace HomeCompa;
using namespace HomeCompa::Flibrary;

namespace
{

class ModeController final : public AbstractTreeViewController::IModeController
{
public:
	static std::unique_ptr<IModeController> Create(const char* const context, std::shared_ptr<ISettings> settings)
	{
		return std::make_unique<ModeController>(context, std::move(settings));
	}

public:
	ModeController(const char* const context, std::shared_ptr<ISettings> settings)
		: m_settingsModeKey { QString(Constant::Settings::VIEW_MODE_KEY_TEMPLATE).arg(context) }
		, m_settings { std::move(settings) }
	{
	}

private: // IModeController
	QString GetMode() const override
	{
		return m_settings->Get(m_settingsModeKey).toString();
	}

	void SetMode(const QString& value) override
	{
		m_settings->Set(m_settingsModeKey, value);
	}

	void SetKey(QString value) override
	{
		m_settingsModeKey = std::move(value);
	}

private:
	QString                                       m_settingsModeKey;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
};

} // namespace

struct AbstractTreeViewController::Impl final
{
	AbstractTreeViewController& self;
	IObserver*                  observer { nullptr };

	explicit Impl(AbstractTreeViewController& self)
		: self(self)
	{
	}
};

AbstractTreeViewController::AbstractTreeViewController(const char* const context, std::shared_ptr<ISettings> settings, const std::shared_ptr<IModelProvider>& modelProvider)
	: m_context { context }
	, m_settings { std::shared_ptr { settings } }
	, m_modeController { ModeController::Create(m_context, std::move(settings)) }
	, m_modelProvider { modelProvider }
	, m_impl { *this }
{
}

AbstractTreeViewController::~AbstractTreeViewController() = default;

const char* AbstractTreeViewController::TrContext() const noexcept
{
	return m_context;
}

int AbstractTreeViewController::GetModeIndex() const
{
	return GetModeIndex(m_modeController->GetMode());
}

void AbstractTreeViewController::RegisterObserver(IObserver* observer)
{
	m_impl->observer = observer;
	Register(observer);
}

void AbstractTreeViewController::UnregisterObserver(IObserver* observer)
{
	m_impl->observer = nullptr;
	Unregister(observer);
}

void AbstractTreeViewController::Setup()
{
	OnModeChanged(m_modeController->GetMode());
}

void AbstractTreeViewController::SetMode(const QString& mode)
{
	m_modeController->SetMode(mode);
	OnModeChanged(mode);
}

void AbstractTreeViewController::RestoreCurrentId()
{
	m_impl->observer->OnRestoreCurrentIdRequested();
}

QAbstractItemModel* AbstractTreeViewController::GetModel() const noexcept
{
	return m_impl->observer ? m_impl->observer->GetModel() : nullptr;
}

QModelIndex AbstractTreeViewController::GetCurrentIndex() const noexcept
{
	return m_impl->observer ? m_impl->observer->GetCurrentIndex() : QModelIndex {};
}
