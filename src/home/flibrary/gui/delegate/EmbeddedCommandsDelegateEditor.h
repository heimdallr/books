#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "BaseDelegateEditor.h"

class QComboBox;

namespace HomeCompa::Flibrary
{

class EmbeddedCommandsDelegateEditor final : public BaseDelegateEditor
{
	NON_COPY_MOVABLE(EmbeddedCommandsDelegateEditor)

public:
	explicit EmbeddedCommandsDelegateEditor(QWidget* parent = nullptr);
	~EmbeddedCommandsDelegateEditor() override;

private: // BaseDelegateEditor
	QString GetText() const override;
	void SetText(const QString& value) override;
	void OnSetModelData(const QString& value) override;

private:
	std::unique_ptr<QComboBox> m_impl;
};

}
