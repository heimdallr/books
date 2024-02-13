#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/theme/ITheme.h"
#include "interface/ui/IMainWindow.h"

#include "logging/init.h"

#include "logic/model/LogModel.h"

#include "di_app.h"

#include "gui/StyleUtils.h"
#include "util/ISettings.h"
#include "util/DyLib.h"
#include "version/AppVersion.h"

#include "config/git_hash.h"
#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

struct Action
{
	QString id;
	QString title;
	bool selected { false };
};

struct ThemeResult
{
	std::shared_ptr<Util::DyLib> lib;
	std::vector<Action> actions;
};

class ThemeRegistrar final : virtual public IThemeRegistrar
{
public:
	ThemeRegistrar(ThemeResult & themeResult, QApplication & app, ISettings & settings)
		: m_themeResult(themeResult)
		, m_app(app)
		, m_settings(settings)
		, m_themeId(m_settings.Get(Constant::Settings::THEME_KEY).toString())
	{
	}

	void SetLib(std::shared_ptr<Util::DyLib> lib, const bool lastLib)
	{
		m_lib = std::move(lib);
		m_lastLib = lastLib;
	}

private: // IThemeRegistrar
	void Register(const ITheme & theme) override
	{
		m_themeResult.actions.emplace_back(theme.GetThemeId(), theme.GetThemeTitle());
		if (!NeedInstall())
			return;

		StyleUtils::EnableSetHeaderViewStyle(m_themeId.isEmpty());
		m_themeResult.actions.back().selected = true;
		m_themeResult.lib = std::move(m_lib);
		m_app.setStyleSheet(theme.GetStyleSheet());
		m_installed = true;

		if (m_themeResult.actions.back().id != m_themeId)
			m_settings.Set(Constant::Settings::THEME_KEY, m_themeResult.actions.back().id);
	}

	bool NeedInstall() const
	{
		return !m_installed && m_lastLib || m_themeResult.actions.back().id == m_themeId;
	}

private:
	ThemeResult & m_themeResult;
	QApplication & m_app;
	ISettings & m_settings;
	QString m_themeId;
	std::shared_ptr<Util::DyLib> m_lib;
	bool m_installed { false };
	bool m_lastLib { false };
};

ThemeResult SetTheme(QApplication & app, ISettings & settings)
{
	StyleUtils::EnableSetHeaderViewStyle(true);
	const auto theme = settings.Get(Constant::Settings::THEME_KEY).toString();

	ThemeResult result;
	ThemeRegistrar registrar(result, app, settings);

	const QDir appDir(QApplication::applicationDirPath());
	const auto themeFiles = appDir.entryList({ "Theme*.dll" }, QDir::Filter::Files);
	for (const auto& file : themeFiles)
	{
		const auto lib = std::make_shared<Util::DyLib>();
		if (!lib->Open(appDir.filePath(file).toStdWString()))
			continue;

		registrar.SetLib(lib, file == themeFiles.back());
		const auto invoker = lib->GetTypedProc<void(IThemeRegistrar&)>("Register");
		invoker(registrar);
	}

	return result;
}

}

int main(int argc, char * argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, PRODUCT_ID).toStdWString());
	LogModelAppender logModelAppender;

	PLOGI << "App started";
	PLOGI << "Version: " << GetApplicationVersion();
	PLOGI << "Commit hash: " << GIT_HASH;

	try
	{
		QApplication app(argc, argv);
		PLOGD << "QApplication created";

		QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

		while (true)
		{
			std::shared_ptr<Hypodermic::Container> container;
			{
				Hypodermic::ContainerBuilder builder;
				DiInit(builder, container);
			}
			PLOGD << "DI-container created";

			const auto themeHolder = SetTheme(app, *container->resolve<ISettings>());

			container->resolve<ITaskQueue>()->Execute();
			const auto logicFactory = container->resolve<ILogicFactory>();
			const auto mainWindow = container->resolve<IMainWindow>();
			for (const auto & [id, title, selected] : themeHolder.actions)
				mainWindow->AddThemeAction(id, title, selected);
			mainWindow->Show();

			if (const auto code = QApplication::exec(); code != Constant::RESTART_APP)
			{
				PLOGI << "App finished with " << code;
				return code;
			}

			PLOGI << "App restarted";
		}
	}
	catch (const std::exception & ex)
	{
		PLOGF << "App failed with " << ex.what();
	}
	catch (...)
	{
		PLOGF << "App failed with unknown error";
	}
}
