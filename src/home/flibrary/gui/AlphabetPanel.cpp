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
#include "GuiUtil/GeometryRestorable.h"

#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto ID = "id";
constexpr auto KEY_TEMPLATE = "ui/View/Alphabets/%1/%2";
constexpr auto VISIBLE = "visible";

QString GetVisibleKey(const QWidget& widget)
{
	return QString(KEY_TEMPLATE).arg(widget.property(ID).toString(), VISIBLE);
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
	Impl(QMainWindow* self, std::shared_ptr<ISettings> settings)
		: GeometryRestorable(*this, settings, "AlphabetPanel")
		, GeometryRestorableObserver(*self)
		, m_settings { std::move(settings) }
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
	}

private:
	void AddToolBar(QMainWindow* self, const QString& id, const QString& alphabet)
	{
		const auto it = m_languages.find(id);
		const auto* lang = it != m_languages.end() ? it->second : nullptr;
		const auto translatedLang = lang ? Loc::Tr(LANGUAGES_CONTEXT, lang) : id;

		auto* toolBar = new ToolBar(id, translatedLang, *self, self);
		toolBar->setVisible(Visible(toolBar));
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
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	std::unordered_map<QString, const char*> m_languages { GetLanguagesMap() };
	ToolBars m_toolBars;
	Ui::AlphabetPanel m_ui;
};

AlphabetPanel::AlphabetPanel(std::shared_ptr<ISettings> settings, QWidget* parent)
	: QMainWindow(parent)
	, m_impl(this, std::move(settings))
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
