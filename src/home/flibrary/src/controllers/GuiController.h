#pragma once

#include <string>

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class ModelController;

class GuiController
	: public QObject
{
	NON_COPY_MOVABLE(GuiController)
	Q_OBJECT
	Q_PROPERTY(bool running READ GetRunning NOTIFY RunningChanged)
public:
	explicit GuiController(const std::string & databaseName, QObject * parent = nullptr);
	~GuiController() override;

	void Start();

public:
	Q_INVOKABLE void OnKeyPressed(int key, int modifiers);
	Q_INVOKABLE ModelController * GetAuthorsModelController() noexcept;

signals:
	void RunningChanged() const;

private: // property setters
	bool GetRunning() const noexcept;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
