#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/IUiFactory.h"
#include "interface/ui/dialogs/IComboBoxTextDialog.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class ComboBoxTextDialog final
	: public QDialog
	, virtual public IComboBoxTextDialog
{
	NON_COPY_MOVABLE(ComboBoxTextDialog)

public:
	ComboBoxTextDialog(const std::shared_ptr<const IUiFactory>& uiFactory, std::shared_ptr<ISettings> settings, QWidget* parent = nullptr);
	~ComboBoxTextDialog() override;

private:
	QDialog&   GetDialog() override;
	QComboBox& GetComboBox() override;
	QLineEdit& GetLineEdit() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
