#pragma once

#include <QMainWindow>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/IAlphabetPanel.h"

namespace HomeCompa::Flibrary
{

class AlphabetPanel final
	: public QMainWindow
	, virtual public IAlphabetPanel
{
	NON_COPY_MOVABLE(AlphabetPanel)

public:
	AlphabetPanel(QWidget* parent = nullptr);
	~AlphabetPanel() override;

private: // IAlphabetPanel
	QWidget* GetWidget() noexcept override;
	const ToolBars& GetToolBars() const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
