#include <QQmlEngine>

#include <plog/Log.h>

#include "ModelControllers/IBooksModelControllerObserver.h"

#include "ComboBoxController.h"

#include "LanguageController.h"

namespace HomeCompa::Flibrary {

struct LanguageController::Impl
	: IBooksModelControllerObserver
	, private IComboBoxDataProvider
	, private ComboBoxObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	ComboBoxController comboBoxController { *this };

	explicit Impl(ILanguageProvider & languageProvider)
		: m_languageProvider(languageProvider)
	{
		QQmlEngine::setObjectOwnership(&comboBoxController, QQmlEngine::CppOwnership);
		comboBoxController.RegisterObserver(this);
	}

	~Impl() override
	{
		comboBoxController.UnregisterObserver(this);
	}

private: // ComboBoxDataProvider
	QString GetValue() const override
	{
		return m_languageProvider.GetLanguage();
	}

	const QString & GetTitleDefault(const QString & /*value*/) const override
	{
		static const QString defaultTitle;
		return defaultTitle;
	}

private: // ComboBoxObserver
	void SetValue(const QString & value) override
	{
		if (m_preventSetLanguageFilter)
			return;

		m_languageProvider.SetLanguage(value);
		comboBoxController.OnValueChanged();
	}

private: //BooksModelControllerObserver
	void HandleBookChanged(const Book &) override
	{
	}

	void HandleModelReset() override
	{
		m_preventSetLanguageFilter = true;
		SimpleModelItems simpleModelItems;
		std::ranges::transform(m_languageProvider.GetLanguages(), std::back_inserter(simpleModelItems), [] (const QString & item)
		{
			return SimpleModelItem { item, item };
		});
		assert(!simpleModelItems.empty() && simpleModelItems.front().Value.isEmpty());
		simpleModelItems.front().Title = QString::fromUtf8("\xE2\x80\x94");
		comboBoxController.SetData(std::move(simpleModelItems));
		m_preventSetLanguageFilter = false;
	}

private:
	ILanguageProvider & m_languageProvider;
	bool m_preventSetLanguageFilter { false };
};

LanguageController::LanguageController(ILanguageProvider & languageProvider, QObject * parent)
	: QObject(parent)
	, m_impl(languageProvider)
{
	PLOGD << "LanguageController created";
}

LanguageController::~LanguageController()
{
	PLOGD << "LanguageController destroyed";
}

IBooksModelControllerObserver * LanguageController::GetBooksModelControllerObserver() noexcept
{
	return m_impl.get();
}

ComboBoxController * LanguageController::GetComboBoxController() noexcept
{
	return &m_impl->comboBoxController;
}

}
