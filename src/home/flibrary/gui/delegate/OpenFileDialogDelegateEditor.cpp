#include "OpenFileDialogDelegateEditor.h"

#include <QAbstractItemModel>

#include "interface/Localization.h"
#include "interface/constants/SettingsConstant.h"

#include "util/files.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT           = "OpenFileDialogDelegateEditor";
constexpr auto APP_FILE_FILTER   = QT_TRANSLATE_NOOP("OpenFileDialogDelegateEditor", "Applications (*.exe);;Scripts (*.bat *.cmd);;All files (*.*)");
constexpr auto FILE_DIALOG_TITLE = QT_TRANSLATE_NOOP("OpenFileDialogDelegateEditor", "Select Application");
constexpr auto DIALOG_KEY        = "Script";

TR_DEF

}

class OpenFileDialogDelegateEditor::Impl : public QLineEdit
{
public:
	Impl(const OpenFileDialogDelegateEditor& self, const ISettings& settings, std::shared_ptr<const Util::IUiFactory> uiFactory)
	{
		const auto* action = addAction(QIcon(":/icons/exe.svg"), TrailingPosition);
		connect(action, &QAction::triggered, this, [&, preferRelative = settings.Get(Constant::Settings::PREFER_RELATIVE_PATHS, false), uiFactory = std::move(uiFactory)] {
			auto fileName = uiFactory->GetOpenFileName(DIALOG_KEY, Tr(FILE_DIALOG_TITLE), Tr(APP_FILE_FILTER));
			if (fileName.isEmpty())
				return;

			if (preferRelative)
				fileName = Util::ToRelativePath(fileName);

			fileName = QDir::toNativeSeparators(fileName);

			setText(fileName);
			self.m_model->setData(self.m_model->index(self.m_row, self.m_column), fileName);
		});
	}
};

OpenFileDialogDelegateEditor::OpenFileDialogDelegateEditor(const std::shared_ptr<const ISettings>& settings, std::shared_ptr<const Util::IUiFactory> uiFactory, QWidget* parent)
	: BaseDelegateEditor(parent)
	, m_impl(*this, *settings, std::move(uiFactory))
{
	SetWidget(m_impl.get());
}

OpenFileDialogDelegateEditor::~OpenFileDialogDelegateEditor() = default;

QString OpenFileDialogDelegateEditor::GetText() const
{
	return m_impl->text();
}

void OpenFileDialogDelegateEditor::SetText(const QString& value)
{
	m_impl->setText(value);
}
