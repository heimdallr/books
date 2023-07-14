#include "TreeViewControllerNavigation.h"

#include <qglobal.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <QString> // for plog
#include <plog/Log.h>

#include "interface/constants/SettingsConstant.h"
#include "util/ISettings.h"
#include "util/ISettingsObserver.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr const char * g_modeNames[]
{
	QT_TRANSLATE_NOOP("Navigation", "Authors"),
	QT_TRANSLATE_NOOP("Navigation", "Series"),
	QT_TRANSLATE_NOOP("Navigation", "Genres"),
	QT_TRANSLATE_NOOP("Navigation", "Archives"),
	QT_TRANSLATE_NOOP("Navigation", "Groups"),
};

}

class TreeViewControllerNavigation::Impl final
	: ISettingsObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(ITreeViewController & self
		, std::shared_ptr<ISettings> settings
	)
		: m_self(self)
		, m_settings(std::move(settings))
	{
		m_settings->RegisterObserver(this);
	}

	~Impl() override
	{
		m_settings->UnregisterObserver(this);
	}

private: // ISettingsObserver
	void HandleValueChanged(const QString & key, const QVariant & value) override
	{
		if (key == m_settingsModeKey)
			PLOGD << value.toString();
	}

private:
	ITreeViewController & m_self;
	std::shared_ptr<ISettings> m_settings;
	QString m_settingsModeKey { QString(Constant::Settings::VIEW_MODE_KEY_TEMPLATE).arg(m_self.GetTreeViewName()) };
};

TreeViewControllerNavigation::TreeViewControllerNavigation(std::shared_ptr<ISettings> settings)
	: m_impl(*this, std::move(settings))
{
}

TreeViewControllerNavigation::~TreeViewControllerNavigation() = default;

const char * TreeViewControllerNavigation::GetTreeViewName() const noexcept
{
	return "Navigation";
}

std::vector<const char *> TreeViewControllerNavigation::GetModeNames() const
{
	return std::vector<const char *>{std::cbegin(g_modeNames), std::cend(g_modeNames)};
}
