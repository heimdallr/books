#include "EmbeddedCommandsDelegateEditor.h"

#include <QComboBox>

#include "fnd/memory.h"

#include "interface/logic/IScriptController.h"

#include "util/localization.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "ScriptController";
constexpr const char* EMBEDDED_COMMANDS[] {
	{ QT_TRANSLATE_NOOP("ScriptController", "Download") },
};
static_assert(std::size(EMBEDDED_COMMANDS) == static_cast<size_t>(IScriptController::EmbeddedCommand::Last));

TR_DEF
}

EmbeddedCommandsDelegateEditor::EmbeddedCommandsDelegateEditor(QWidget* parent)
	: BaseDelegateEditor(parent)
	, m_impl { std::make_unique<QComboBox>() }
{
	for (const auto* name : EMBEDDED_COMMANDS)
		m_impl->addItem(Tr(name), QVariant { QString { name } });
	m_impl->setCurrentIndex(-1);
	SetWidget(m_impl.get());

	PLOGV << "EmbeddedCommandsDelegateEditor created";
}

EmbeddedCommandsDelegateEditor::~EmbeddedCommandsDelegateEditor()
{
	PLOGV << "EmbeddedCommandsDelegateEditor destroyed";
}

QString EmbeddedCommandsDelegateEditor::GetText() const
{
	return m_impl->currentData().toString();
}

void EmbeddedCommandsDelegateEditor::SetText(const QString& value)
{
	PLOGD << "SetText" << value;
	if (const auto index = m_impl->findData(value); index >= 0)
		m_impl->setCurrentIndex(index);
	else
		m_impl->setCurrentIndex(-1);
}

void EmbeddedCommandsDelegateEditor::OnSetModelData(const QString& value)
{
	PLOGD << "SetText" << value;
	SetText(value);
}
