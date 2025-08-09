#include "ui_AlphabetPanel.h"

#include "AlphabetPanel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QTimer>
#include <QToolBar>

#include "fnd/observable.h"

#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

class ToolBar final : public QToolBar
{
public:
	ToolBar(const QString& id, const QString& title, QMainWindow& mainWindow, QWidget* parent = nullptr)
		: QToolBar(title, parent)
	{
		setMovable(false);
		setProperty(IAlphabetPanel::ID, id);
		setProperty(IAlphabetPanel::TITLE, title);
		setProperty(IAlphabetPanel::VISIBLE, "true");
		mainWindow.addToolBar(Qt::ToolBarArea::TopToolBarArea, this);
		mainWindow.addToolBarBreak(Qt::ToolBarArea::TopToolBarArea);

		connect(this, &QToolBar::visibilityChanged, [this](const bool visible) { setProperty(IAlphabetPanel::VISIBLE, visible); });

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

class AlphabetPanel::Impl final : public Observable<IObserver>
{
public:
	Impl(QMainWindow* self)
	{
		m_ui.setupUi(self);

		QFile file(":/data/alphabet.json");
		[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
		assert(ok);

		QJsonParseError parseError;
		const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
		assert(parseError.error == QJsonParseError::NoError && doc.isArray());

		for (const auto alphabetValue : doc.array())
		{
			assert(alphabetValue.isObject());
			const auto alphabetObject = alphabetValue.toObject();
			AddToolBar(self, alphabetObject[ID].toString(), alphabetObject["data"].toString());
		}
	}

	const ToolBars& GetToolBars() const
	{
		return m_toolBars;
	}

private:
	void AddToolBar(QMainWindow* self, const QString& id, const QString& alphabet)
	{
		const auto it = m_languages.find(id);
		const auto* lang = it != m_languages.end() ? it->second : nullptr;
		const auto translatedLang = lang ? Loc::Tr(LANGUAGES_CONTEXT, lang) : id;

		auto* toolBar = new ToolBar(id, translatedLang, *self, self);
		for (const auto& ch : alphabet)
		{
			auto* action = toolBar->addAction(ch);
			action->setToolTip({});
			action->setStatusTip({});
			connect(action, &QAction::triggered, CreateLetterClickFunctor(ch));
		}
		m_toolBars.emplace_back(toolBar);
	}

private:
	std::unordered_map<QString, const char*> m_languages { GetLanguagesMap() };
	Ui::AlphabetPanel m_ui;
	ToolBars m_toolBars;
};

AlphabetPanel::AlphabetPanel(QWidget* parent)
	: QMainWindow(parent)
	, m_impl(this)
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

void AlphabetPanel::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void AlphabetPanel::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
