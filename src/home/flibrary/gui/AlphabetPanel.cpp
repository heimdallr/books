#include "ui_AlphabetPanel.h"

#include "AlphabetPanel.h"

#include <ranges>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QTimer>
#include <QToolBar>

#include "fnd/observable.h"

#include "interface/localization.h"

#include "gutil/GeometryRestorable.h"
#include "util/language.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT               = "AlphabetPanel";
constexpr auto SELECT_LANGUAGE_TITLE = QT_TRANSLATE_NOOP("AlphabetPanel", "Specify the language of the new alphabet");
constexpr auto SELECT_LANGUAGE_LABEL = QT_TRANSLATE_NOOP("AlphabetPanel", "Select language");
constexpr auto SELECT_ALPHABET_TITLE = QT_TRANSLATE_NOOP("AlphabetPanel", "Specify the letters of the new alphabet");
constexpr auto SELECT_ALPHABET_LABEL = QT_TRANSLATE_NOOP("AlphabetPanel", "Type letters");

TR_DEF

constexpr auto ID             = "id";
constexpr auto GROUP_KEY      = "ui/View/Alphabets";
constexpr auto LINKED_CONTROL = "Preferences/Alphabets/LinkedControl";
constexpr auto KEY_TEMPLATE   = "ui/View/Alphabets/%1/%2";
constexpr auto VISIBLE        = "visible";
constexpr auto ORD_NUM        = "order";
constexpr auto ALPHABET       = "alphabet";

QString GetVisibleKey(const QWidget& widget)
{
	return QString(KEY_TEMPLATE).arg(widget.property(ID).toString(), VISIBLE);
}

QString GetOrdNumKey(const QWidget& widget)
{
	return QString(KEY_TEMPLATE).arg(widget.property(ID).toString(), ORD_NUM);
}

class ToolBar final : public QToolBar
{
public:
	ToolBar(const QString& id, const QString& title, QMainWindow& mainWindow, QWidget* parent = nullptr)
		: QToolBar(title, parent)
	{
		setMovable(false);
		setProperty(ID, id);
		setAccessibleName(title);
		mainWindow.addToolBar(Qt::ToolBarArea::TopToolBarArea, this);
		mainWindow.addToolBarBreak(Qt::ToolBarArea::TopToolBarArea);

		qApp->installEventFilter(this);
	}

private:
	bool eventFilter(QObject* obj, QEvent* event) override
	{
		return event->type() == QEvent::ToolTip && obj->parent() && obj->parent() == this;
	}
};

class IControlGetter // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IControlGetter()      = default;
	virtual QLineEdit* Get() const = 0;
};

auto CreateLetterClickFunctor(const QChar ch, const IControlGetter* controlGetter)
{
	return [=] {
		auto* edit = controlGetter->Get();
		if (!edit)
			return;

		const auto text     = edit->text();
		const auto position = edit->cursorPosition();
		edit->setText(text.first(position) + ch + text.last(text.length() - position));
		edit->setCursorPosition(position + 1);
	};
}

} // namespace

class AlphabetPanel::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, public Observable<IObserver>
	, IControlGetter
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(QMainWindow& self, std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<ISettings> settings)
		: GeometryRestorable(*this, settings, "AlphabetPanel")
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_uiFactory { std::move(uiFactory) }
		, m_settings { std::move(settings) }
	{
		m_ui.setupUi(&self);

		QFile                       file(":/data/alphabet.json");
		[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
		assert(ok);

		QJsonParseError parseError;
		const auto      doc = QJsonDocument::fromJson(file.readAll(), &parseError);
		assert(parseError.error == QJsonParseError::NoError && doc.isArray());

		std::ranges::for_each(doc.array(), [this](const auto alphabetValue) {
			assert(alphabetValue.isObject());
			const auto alphabetObject = alphabetValue.toObject();
			AddToolBar(m_self, alphabetObject[ID].toString(), alphabetObject["data"].toString());
		});

		const auto customIds = [this] {
			QStringList                 ids;
			const SettingsGroup         group(*m_settings, GROUP_KEY);
			std::unordered_set<QString> unique;
			std::ranges::transform(m_toolBars, std::inserter(unique, unique.end()), [](const auto* item) {
				return item->property(ID).toString();
			});
			std::ranges::copy(
				m_settings->GetGroups() | std::views::filter([&](const auto& item) {
					return !unique.contains(item);
				}),
				std::back_inserter(ids)
			);
			return ids;
		}();

		std::ranges::for_each(customIds, [this](const auto& id) {
			AddToolBar(m_self, id, m_settings->Get(QString(KEY_TEMPLATE).arg(id, ALPHABET)).toString(), true);
		});

		std::ranges::sort(m_toolBars, {}, [](const auto* toolBar) {
			return toolBar->property(ORD_NUM).toInt();
		});

		if (m_linkedControl.isEmpty())
			connect(qApp, &QApplication::focusChanged, &m_self, [this](QWidget*, QWidget* now) {
				m_self.setEnabled(qobject_cast<QLineEdit*>(now));
			});

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
	}

	void OnShow() const
	{
		QTimer::singleShot(0, [this] {
			const auto height = std::accumulate(m_toolBars.cbegin(), m_toolBars.cend(), 0, [](const int init, const auto& toolBar) {
				return init + (toolBar->isVisible() ? toolBar->height() : 0);
			});
			m_self.setMinimumHeight(height);
			m_self.setMaximumHeight(height);
		});
	}

	const ToolBars& GetToolBars() const
	{
		return m_toolBars;
	}

	bool Visible(const QToolBar* toolBar) const
	{
		return m_settings->Get(GetVisibleKey(*toolBar), false);
	}

	void SetVisible(QToolBar* toolBar, const bool visible)
	{
		m_settings->Set(GetVisibleKey(*toolBar), visible);

		if (visible)
		{
			for (auto* tb : m_toolBars)
			{
				QSignalBlocker blocker(tb);
				m_self.removeToolBar(tb);
			}

			std::erase_if(m_toolBars, [=](const auto* item) {
				return item == toolBar;
			});
			m_toolBars.emplace_back(toolBar);
			for (int n = 0; auto* tb : m_toolBars)
			{
				QSignalBlocker blocker(tb);
				m_self.addToolBar(Qt::ToolBarArea::TopToolBarArea, tb);
				m_self.addToolBarBreak(Qt::ToolBarArea::TopToolBarArea);
				tb->setVisible(Visible(tb));
				m_settings->Set(GetOrdNumKey(*tb), ++n);
			}

			Perform(&IAlphabetPanel::IObserver::OnToolBarChanged);
		}

		QTimer::singleShot(0, [this, toolBar, visible] {
			toolBar->setVisible(visible);
			OnShow();
		});
	}

	void AddNewAlphabet()
	{
		QStringList items;

		std::unordered_set<QString> keysFilter { UNDEFINED_KEY };
		std::ranges::transform(m_toolBars, std::inserter(keysFilter, keysFilter.end()), [](const auto* toolBar) {
			return toolBar->property(ID).toString();
		});

		std::map<std::pair<int, QString>, const char*> languages;
		std::ranges::transform(
			LANGUAGES | std::views::filter([&](const Language& language) {
				return !keysFilter.contains(language.key);
			}),
			std::inserter(languages, languages.end()),
			[](const Language& language) {
				return std::make_pair(std::make_pair(language.priority, Loc::Tr(LANGUAGES_CONTEXT, language.title)), language.key);
			}
		);

		items.reserve(static_cast<int>(languages.size()));

		std::ranges::copy(languages | std::views::keys | std::views::values, std::back_inserter(items));
		const auto title = m_uiFactory->GetText(Tr(SELECT_LANGUAGE_TITLE), Tr(SELECT_LANGUAGE_LABEL), {}, items);
		const auto it    = std::ranges::find(languages, title, [](const auto& item) {
            return item.first.second;
        });
		assert(it != languages.end());

		const auto alphabet = m_uiFactory->GetText(Tr(SELECT_ALPHABET_TITLE), Tr(SELECT_ALPHABET_LABEL));

		const auto keyTemplate = QString(KEY_TEMPLATE).arg(it->second, "%1");
		m_settings->Set(keyTemplate.arg(VISIBLE), true);
		m_settings->Set(keyTemplate.arg(ORD_NUM), m_toolBars.size() + 1);
		m_settings->Set(keyTemplate.arg(ALPHABET), alphabet);

		AddToolBar(m_self, it->second, alphabet, true);
		Perform(&IAlphabetPanel::IObserver::OnToolBarChanged);
		OnShow();
	}

private: // Util::GeometryRestorableObserver
	void OnFontChanged(const QFont&) override
	{
		OnShow();
	}

private: // ILinkedControlGetter
	QLineEdit* Get() const override
	{
		return std::invoke(m_controlGetter, this);
	}

private:
	void AddToolBar(QMainWindow& self, const QString& id, const QString& alphabet, const bool removable = false)
	{
		const auto  it             = m_languages.find(id);
		const auto* lang           = it != m_languages.end() ? it->second : nullptr;
		const auto  translatedLang = lang ? Loc::Tr(LANGUAGES_CONTEXT, lang) : id;

		auto* toolBar = new ToolBar(id, translatedLang, self, &self);
		toolBar->setVisible(Visible(toolBar));
		toolBar->setFont(self.font());

		const auto createChar = [&](const QChar ch) {
			auto* action = toolBar->addAction(ch);
			action->setFont(self.font());
			connect(action, &QAction::triggered, CreateLetterClickFunctor(ch, this));
		};

		std::ranges::for_each(alphabet, createChar);

		if (removable)
		{
			auto* spacer = new QWidget(toolBar);
			spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			toolBar->addWidget(spacer);

			auto* removeAction = toolBar->addAction(QIcon(":/icons/remove.svg"), "");
			connect(removeAction, &QAction::triggered, [this, toolBar] {
				const QSignalBlocker blocker(toolBar);
				m_self.removeToolBar(toolBar);
				std::erase_if(m_toolBars, [&](const auto* item) {
					return item == toolBar;
				});
				m_settings->Remove(QString("%1/%2").arg(GROUP_KEY, toolBar->property(ID).toString()));
				Perform(&IAlphabetPanel::IObserver::OnToolBarChanged);
				OnShow();
			});
		}

		m_toolBars.emplace_back(toolBar);
		toolBar->setProperty(ORD_NUM, m_settings->Get(GetOrdNumKey(*toolBar), m_toolBars.size()));
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	QLineEdit* GetFocusedControl() const
	{
		return qobject_cast<QLineEdit*>(QApplication::focusWidget());
	}

	QLineEdit* GetLinkedControl() const
	{
		auto*      parent   = m_uiFactory->GetParentWidget();
		const auto children = parent->findChildren<QLineEdit*>();
		const auto it       = std::ranges::find(children, m_linkedControl, [](const auto* item) {
            return item->accessibleName();
        });
		if (it == children.end())
			return nullptr;

		auto* control = *it;
		if (!control->hasFocus())
			control->setFocus(Qt::FocusReason::OtherFocusReason);

		return control;
	}

private:
	QMainWindow&                                  m_self;
	std::shared_ptr<const IUiFactory>             m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	const QString                                 m_linkedControl { m_settings->Get(LINKED_CONTROL, QString {}) };
	using ControlGetter = QLineEdit* (Impl::*)() const;
	const ControlGetter                      m_controlGetter { m_linkedControl.isEmpty() ? &Impl::GetFocusedControl : &Impl::GetLinkedControl };
	std::unordered_map<QString, const char*> m_languages { GetLanguagesMap() };
	ToolBars                                 m_toolBars;
	Ui::AlphabetPanel                        m_ui;
};

AlphabetPanel::AlphabetPanel(std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<ISettings> settings, QWidget* parent)
	: QMainWindow(parent)
	, m_impl(*this, std::move(uiFactory), std::move(settings))
{
}

AlphabetPanel::~AlphabetPanel() = default;

void AlphabetPanel::showEvent(QShowEvent*)
{
	m_impl->OnShow();
}

QWidget* AlphabetPanel::GetWidget() noexcept
{
	return this;
}

const IAlphabetPanel::ToolBars& AlphabetPanel::GetToolBars() const
{
	return m_impl->GetToolBars();
}

bool AlphabetPanel::Visible(const QToolBar* toolBar) const
{
	return m_impl->Visible(toolBar);
}

void AlphabetPanel::SetVisible(QToolBar* toolBar, const bool visible)
{
	m_impl->SetVisible(toolBar, visible);
}

void AlphabetPanel::AddNewAlphabet()
{
	m_impl->AddNewAlphabet();
}

void AlphabetPanel::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void AlphabetPanel::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
