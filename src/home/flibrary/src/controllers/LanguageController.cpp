#include <QQmlEngine>

#include "ModelControllers/BooksModelControllerObserver.h"

#include "ComboBoxController.h"

#include "LanguageController.h"

namespace HomeCompa::Flibrary {

struct LanguageController::Impl
	: BooksModelControllerObserver
	, private ComboBoxDataProvider
	, private ComboBoxObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	ComboBoxController comboBoxController { *this };

	explicit Impl(LanguageProvider & languageProvider)
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
	void HandleBookChanged(const std::string & /*folder*/, const std::string & /*file*/) override
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
	LanguageProvider & m_languageProvider;
	bool m_preventSetLanguageFilter { false };
};

LanguageController::LanguageController(LanguageProvider & languageProvider, QObject * parent)
	: QObject(parent)
	, m_impl(languageProvider)
{
}

LanguageController::~LanguageController() = default;

BooksModelControllerObserver * LanguageController::GetBooksModelControllerObserver() noexcept
{
	return m_impl.get();
}

ComboBoxController * LanguageController::GetComboBoxController() noexcept
{
	return &m_impl->comboBoxController;
}

}
