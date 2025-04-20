#include "LocaleController.h"

#include <QActionGroup>
#include <QMenu>

#include "interface/constants/Localization.h"

#include "util/SortString.h"

#include "log.h"

namespace HomeCompa::Flibrary
{

namespace
{
constexpr auto LOCALE = "ui/locale";
constexpr auto NAME = "name";

}

class LocaleController::Impl
{
public:
	explicit Impl(LocaleController& self, std::shared_ptr<ISettings> settings, std::shared_ptr<IUiFactory> uiFactory)
		: m_self(self)
		, m_settings(std::move(settings))
		, m_uiFactory(std::move(uiFactory))
	{
		m_actionGroup.setExclusive(true);
	}

	void Setup(QMenu& menu)
	{
		const auto locales = Loc::GetLocales();
		if (locales.empty())
			return;

		if (locales.size() == 1)
			return Util::QStringWrapper::SetLocale(locales.front());

		const auto currentLocale = Loc::GetLocale(*m_settings);
		Util::QStringWrapper::SetLocale(currentLocale);
		for (const auto* locale : locales)
		{
			auto* action = menu.addAction(Loc::Tr(Loc::Ctx::LANG, locale), [&, locale] { SetLocale(locale); });
			action->setProperty(NAME, QString(locale));
			m_actionGroup.addAction(action);
			action->setCheckable(true);
			action->setChecked(currentLocale == locale);
			action->setEnabled(currentLocale != locale);
		}
	}

private:
	void SetLocale(const QString& locale)
	{
		for (auto* action : m_actionGroup.actions())
			action->setEnabled(action->property(NAME).toString() != locale);

		m_settings->Set(LOCALE, locale);

		if (m_uiFactory->ShowQuestion(Loc::Tr(Loc::Ctx::COMMON, Loc::CONFIRM_RESTART), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
			emit m_self.LocaleChanged();
	}

private:
	LocaleController& m_self;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	QActionGroup m_actionGroup { nullptr };
};

LocaleController::LocaleController(std::shared_ptr<ISettings> settings, std::shared_ptr<IUiFactory> uiFactory, QObject* parent)
	: QObject(parent)
	, m_impl(*this, std::move(settings), std::move(uiFactory))
{
	PLOGV << "LocaleController created";
}

LocaleController::~LocaleController()
{
	PLOGV << "LocaleController destroyed";
}

void LocaleController::Setup(QMenu& menu)
{
	m_impl->Setup(menu);
}

} // namespace HomeCompa::Flibrary
