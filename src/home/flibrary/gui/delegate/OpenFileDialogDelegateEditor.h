#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "BaseDelegateEditor.h"

namespace HomeCompa::Flibrary
{

class OpenFileDialogDelegateEditor final : public BaseDelegateEditor
{
	NON_COPY_MOVABLE(OpenFileDialogDelegateEditor)

public:
	explicit OpenFileDialogDelegateEditor(std::shared_ptr<const class IUiFactory> uiFactory, QWidget* parent = nullptr);
	~OpenFileDialogDelegateEditor() override;

private: // BaseDelegateEditor
	QString GetText() const override;
	void SetText(const QString& value) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
