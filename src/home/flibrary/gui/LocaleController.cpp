#include "LocaleController.h"

#include <QActionGroup>
#include <QMenu>
#include <QTranslator>

#include <plog/Log.h>

#include "util/ISettings.h"
#include "util/KeyboardLayout.h"

#include "interface/constants/Localization.h"
#include "interface/logic/SortString.h"
#include "interface/ui/IUiFactory.h"

namespace HomeCompa::Flibrary {

namespace {
constexpr auto LOCALE = "ui/locale";
constexpr auto CONTEXT = "LocaleController";
}

class LocaleController::Impl
{
public:
	explicit Impl(LocaleController & self
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IUiFactory> uiFactory
	)
		: m_self(self)
		, m_settings(std::move(settings))
		, m_uiFactory(std::move(uiFactory))
		, m_translators(Loc::LoadLocales(*m_settings))
	{
		m_actionGroup.setExclusive(true);
	}

	void Setup(QMenu & menu)
	{
		const auto currentLocale = Loc::GetLocale(*m_settings);
		QStringWrapper::SetLocale(currentLocale);
		SetKeyboardLayout(currentLocale.toStdString());
		for (const auto * locale : Loc::LOCALES)
		{
			auto * action = menu.addAction(Loc::Tr(Loc::Ctx::LANG, locale), [&, locale]
			{
				SetLocale(locale);
			});
			m_actionGroup.addAction(action);
			action->setCheckable(true);
			action->setChecked(currentLocale == locale);
		}
	}

private:
	void SetLocale(const QString & locale)
	{
		m_settings->Set(LOCALE, locale);

		if (m_uiFactory->ShowQuestion(Loc::Tr(Loc::Ctx::COMMON, Loc::CONFIRM_RESTART), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
			emit m_self.LocaleChanged();
	}

private:
	LocaleController & m_self;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	std::vector<PropagateConstPtr<QTranslator>> m_translators;
	QActionGroup m_actionGroup { nullptr };
};

LocaleController::LocaleController(std::shared_ptr<ISettings> settings
	, std::shared_ptr<IUiFactory> uiFactory
	, QObject * parent
)
	: QObject(parent)
	, m_impl(*this, std::move(settings), std::move(uiFactory))
{
	PLOGD << "LocaleController created";
}

LocaleController::~LocaleController()
{
	PLOGD << "LocaleController destroyed";
}

void LocaleController::Setup(QMenu & menu)
{
	m_impl->Setup(menu);
}

}
