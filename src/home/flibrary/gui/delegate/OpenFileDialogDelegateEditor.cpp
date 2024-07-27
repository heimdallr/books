#include "OpenFileDialogDelegateEditor.h"

#include <QAbstractItemModel>

#include "interface/constants/Localization.h"
#include "interface/ui/IUiFactory.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "OpenFileDialogDelegateEditor";
constexpr auto APP_FILE_FILTER = QT_TRANSLATE_NOOP("OpenFileDialogDelegateEditor", "Applications (*.exe);;Scripts (*.bat *.cmd);;All files (*.*)");
constexpr auto FILE_DIALOG_TITLE = QT_TRANSLATE_NOOP("OpenFileDialogDelegateEditor", "Select Application");
constexpr auto DIALOG_KEY = "Script";

TR_DEF

}

class OpenFileDialogDelegateEditor::Impl : public QLineEdit
{
public:
	Impl(const OpenFileDialogDelegateEditor& self, std::shared_ptr<const IUiFactory> uiFactory)
	{
		const auto* action = addAction(QIcon(":/icons/exe.png"), TrailingPosition);
		connect(action, &QAction::triggered, this, [&, uiFactory = std::move(uiFactory)]
		{
			const auto fileName = QDir::toNativeSeparators(uiFactory->GetOpenFileName(DIALOG_KEY, Tr(FILE_DIALOG_TITLE), Tr(APP_FILE_FILTER)));
			if (fileName.isEmpty())
				return;

			setText(fileName);
			self.m_model->setData(self.m_model->index(self.m_row, self.m_column), fileName);
		});
	}
};

OpenFileDialogDelegateEditor::OpenFileDialogDelegateEditor(std::shared_ptr<const IUiFactory> uiFactory
	, QWidget * parent
)
	: BaseDelegateEditor(parent)
	, m_impl(*this, std::move(uiFactory))
{
	SetWidget(m_impl.get());
}

OpenFileDialogDelegateEditor::~OpenFileDialogDelegateEditor() = default;

QString OpenFileDialogDelegateEditor::GetText() const
{
	return m_impl->text();
}

void OpenFileDialogDelegateEditor::SetText(const QString & value)
{
	m_impl->setText(value);
}
