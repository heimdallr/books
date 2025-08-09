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

#include "GuiUtil/GeometryRestorable.h"
#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "AlphabetPanel";
constexpr auto SELECT_LANGUAGE_TITLE = "Select new alphabet language";
constexpr auto SELECT_LANGUAGE_LABEL = "Select language";
constexpr auto SELECT_ALPHABET_TITLE = "Select new alphabet letters";
constexpr auto SELECT_ALPHABET_LABEL = "Type letters";

TR_DEF

constexpr auto ID = "id";
constexpr auto GROUP_KEY = "ui/View/Alphabets";
constexpr auto KEY_TEMPLATE = "ui/View/Alphabets/%1/%2";
constexpr auto VISIBLE = "visible";
constexpr auto ORD_NUM = "order";
constexpr auto ALPHABET = "alphabet";

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

auto CreateLetterClickFunctor(QChar ch)
{
	return [ch]
	{
		auto* edit = qobject_cast<QLineEdit*>(QApplication::focusWidget());
		if (!edit)
			return;

		const auto text = edit->text();
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

		QFile file(":/data/alphabet.json");
		[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
		assert(ok);

		QJsonParseError parseError;
		const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
		assert(parseError.error == QJsonParseError::NoError && doc.isArray());

		std::ranges::for_each(doc.array(),
		                      [this](const auto alphabetValue)
		                      {
								  assert(alphabetValue.isObject());
								  const auto alphabetObject = alphabetValue.toObject();
								  AddToolBar(m_self, alphabetObject[ID].toString(), alphabetObject["data"].toString());
							  });

		const auto customIds = [this]
		{
			QStringList ids;
			const SettingsGroup group(*m_settings, GROUP_KEY);
			std::unordered_set<QString> unique;
			std::ranges::transform(m_toolBars, std::inserter(unique, unique.end()), [](const auto* item) { return item->property(ID).toString(); });
			std::ranges::copy(m_settings->GetGroups() | std::views::filter([&](const auto& item) { return !unique.contains(item); }), std::back_inserter(ids));
			return ids;
		}();

		std::ranges::for_each(customIds, [this](const auto& id) { AddToolBar(m_self, id, m_settings->Get(QString(KEY_TEMPLATE).arg(id, ALPHABET)).toString()); });

		std::ranges::sort(m_toolBars, {}, [](const auto* toolBar) { return toolBar->property(ORD_NUM).toInt(); });

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
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
		QTimer::singleShot(0, [=] { toolBar->setVisible(visible); });
	}

	void AddNewAlphabet()
	{
		QStringList items;

		std::unordered_set<QString> keysFilter { UNDEFINED_KEY };
		std::ranges::transform(m_toolBars, std::inserter(keysFilter, keysFilter.end()), [](const auto* toolBar) { return toolBar->property(ID).toString(); });

		std::map<std::pair<int, QString>, const char*> languages;
		std::ranges::transform(LANGUAGES | std::views::filter([&](const Language& language) { return !keysFilter.contains(language.key); }),
		                       std::inserter(languages, languages.end()),
		                       [](const Language& language) { return std::make_pair(std::make_pair(language.priority, Loc::Tr(LANGUAGES_CONTEXT, language.title)), language.key); });

		items.reserve(static_cast<int>(languages.size()));

		std::ranges::copy(languages | std::views::keys | std::views::values, std::back_inserter(items));
		const auto title = m_uiFactory->GetText(Tr(SELECT_LANGUAGE_TITLE), Tr(SELECT_LANGUAGE_LABEL), {}, items);
		const auto it = std::ranges::find(languages, title, [](const auto& item) { return item.first.second; });
		assert(it != languages.end());

		const auto alphabet = m_uiFactory->GetText(Tr(SELECT_ALPHABET_TITLE), Tr(SELECT_ALPHABET_LABEL));

		const auto keyTemplate = QString(KEY_TEMPLATE).arg(it->second, "%1");
		m_settings->Set(keyTemplate.arg(VISIBLE), true);
		m_settings->Set(keyTemplate.arg(ORD_NUM), m_toolBars.size() + 1);
		m_settings->Set(keyTemplate.arg(ALPHABET), alphabet);

		AddToolBar(m_self, it->second, alphabet);
		Perform(&IAlphabetPanel::IObserver::OnToolBarChanged);
	}

private:
	void AddToolBar(QMainWindow& self, const QString& id, const QString& alphabet)
	{
		const auto it = m_languages.find(id);
		const auto* lang = it != m_languages.end() ? it->second : nullptr;
		const auto translatedLang = lang ? Loc::Tr(LANGUAGES_CONTEXT, lang) : id;

		auto* toolBar = new ToolBar(id, translatedLang, self, &self);
		toolBar->setVisible(Visible(toolBar));
		toolBar->setFont(self.font());
		for (const auto& ch : alphabet)
		{
			auto* action = toolBar->addAction(ch);
			action->setFont(self.font());
			connect(action, &QAction::triggered, CreateLetterClickFunctor(ch));
		}
		m_toolBars.emplace_back(toolBar);
		toolBar->setProperty(ORD_NUM, m_settings->Get(GetOrdNumKey(*toolBar), m_toolBars.size()));
	}

private:
	QMainWindow& m_self;
	std::shared_ptr<const IUiFactory> m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	std::unordered_map<QString, const char*> m_languages { GetLanguagesMap() };
	ToolBars m_toolBars;
	Ui::AlphabetPanel m_ui;
};

AlphabetPanel::AlphabetPanel(std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<ISettings> settings, QWidget* parent)
	: QMainWindow(parent)
	, m_impl(*this, std::move(uiFactory), std::move(settings))
{
}

AlphabetPanel::~AlphabetPanel() = default;

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
