#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "gutil/interface/IUiFactory.h"
#include "util/ISettings.h"

#include "BaseDelegateEditor.h"

namespace HomeCompa::Flibrary
{

class OpenFileDialogDelegateEditor final : public BaseDelegateEditor
{
	NON_COPY_MOVABLE(OpenFileDialogDelegateEditor)

public:
	OpenFileDialogDelegateEditor(const std::shared_ptr<const ISettings>& settings, std::shared_ptr<const Util::IUiFactory> uiFactory, QWidget* parent = nullptr);
	~OpenFileDialogDelegateEditor() override;

private: // BaseDelegateEditor
	QString GetText() const override;
	void    SetText(const QString& value) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
