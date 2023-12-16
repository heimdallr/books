#include "ui_OpenFileDialogDelegateEditor.h"
#include "OpenFileDialogDelegateEditor.h"

#include <QAbstractItemModel>

#include "interface/constants/Localization.h"
#include "interface/ui/IUiFactory.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "OpenFileDialogDelegateEditor";
constexpr auto APP_FILE_FILTER = QT_TRANSLATE_NOOP("OpenFileDialogDelegateEditor", "Applications (*.exe);;Scripts (*.bat *.cmd);;All files (*.*)");
constexpr auto FILE_DIALOG_TITLE = QT_TRANSLATE_NOOP("OpenFileDialogDelegateEditor", "Select Application");

TR_DEF

}

struct OpenFileDialogDelegateEditor::Impl
{
	Ui::OpenFileDialogDelegateEditor ui {};

	Impl(OpenFileDialogDelegateEditor & self, std::shared_ptr<const IUiFactory> uiFactory)
	{
		ui.setupUi(&self);
		ui.edit->setFocus(Qt::FocusReason::TabFocusReason);
		connect(ui.button, &QAbstractButton::clicked, &self, [&, uiFactory = std::move(uiFactory)]
		{
			const auto fileName = QDir::toNativeSeparators(uiFactory->GetOpenFileName(Tr(FILE_DIALOG_TITLE), {}, Tr(APP_FILE_FILTER)));
			if (fileName.isEmpty())
				return;

			ui.edit->setText(fileName);
			self.m_model->setData(self.m_model->index(self.m_row, self.m_column), fileName);
		});
	}
};

OpenFileDialogDelegateEditor::OpenFileDialogDelegateEditor(std::shared_ptr<const IUiFactory> uiFactory
	, QWidget * parent
)
	: BaseDelegateEditor(this, parent)
	, m_impl(*this, std::move(uiFactory))
{
}

OpenFileDialogDelegateEditor::~OpenFileDialogDelegateEditor() = default;

QString OpenFileDialogDelegateEditor::GetText() const
{
	return m_impl->ui.edit->text();
}

void OpenFileDialogDelegateEditor::SetText(const QString & value)
{
	m_impl->ui.edit->setText(value);
}
