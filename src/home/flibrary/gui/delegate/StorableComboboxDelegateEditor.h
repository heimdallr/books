#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "BaseDelegateEditor.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class StorableComboboxDelegateEditor final
	: public BaseDelegateEditor
{
	NON_COPY_MOVABLE(StorableComboboxDelegateEditor)

public:
	explicit StorableComboboxDelegateEditor(std::shared_ptr<ISettings> settings
		, QWidget * parent = nullptr
	);
	~StorableComboboxDelegateEditor() override;

public:
	void SetSettingsKey(QString key);

private: // BaseDelegateEditor
	QString GetText() const override;
	void SetText(const QString & value) override;
	void SetParent(QWidget * parent) override;
	void OnSetModelData(const QString & value) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

}
