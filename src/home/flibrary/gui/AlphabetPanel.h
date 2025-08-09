#pragma once

#include <QMainWindow>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/IAlphabetPanel.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class AlphabetPanel final
	: public QMainWindow
	, virtual public IAlphabetPanel
{
	NON_COPY_MOVABLE(AlphabetPanel)

public:
	explicit AlphabetPanel(std::shared_ptr<ISettings> settings, QWidget* parent = nullptr);
	~AlphabetPanel() override;

private: // IAlphabetPanel
	QWidget* GetWidget() noexcept override;
	const ToolBars& GetToolBars() const override;
	bool Visible(const QToolBar* toolBar) const override;
	void SetVisible(QToolBar* toolBar, bool visible) override;
	void AddNewAlphabet() override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
