#include "LocaleController.h"

#include <QActionGroup>
#include <QDir>
#include <QTranslator>
#include <QMenu>

#include <plog/Log.h>

#include "util/ISettings.h"

#include "interface/constants/Localization.h"
#include "interface/ui/IUiFactory.h"

namespace HomeCompa::Flibrary {

namespace {
constexpr auto LOCALE = "ui/locale";
constexpr auto CONTEXT = "LocaleController";
constexpr auto CONFIRM_RESTART = QT_TRANSLATE_NOOP("LocaleController", "You must restart the application to apply the changes.\nRestart now?");
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
	{
		m_actionGroup.setExclusive(true);
		const QDir dir = QCoreApplication::applicationDirPath() + "/locales";
		for (const auto & file : dir.entryList(QStringList() << QString("*_%1.qm").arg(GetLocale()), QDir::Files))
		{
			const auto fileName = dir.absoluteFilePath(file);
			auto & translator = m_translators.emplace_back();
			if (translator->load(fileName))
				QCoreApplication::installTranslator(translator.get());
			else
				PLOGE << "Cannot load " << fileName;
		}
	}

	void Setup(QMenu & menu)
	{
		const auto currentLocale = GetLocale();
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
	QString GetLocale() const
	{
		if (auto locale = m_settings->Get(LOCALE).toString(); !locale.isEmpty())
			return locale;

		if (const auto it = std::ranges::find_if(Loc::LOCALES, [sysLocale = QLocale::system().name()] (const char * item)
		{
			return sysLocale.startsWith(item);
		}); it != std::cend(Loc::LOCALES))
			return *it;

		assert(!std::empty(Loc::LOCALES));
		return Loc::LOCALES[0];
	}

	void SetLocale(const QString & locale)
	{
		m_settings->Set(LOCALE, locale);

		if (m_uiFactory->ShowQuestion(Loc::Tr(CONTEXT, CONFIRM_RESTART), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
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
