#pragma once

#include <string>

#include <QObject>

#include "fnd/memory.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class GuiController
	: public QObject
{
	Q_OBJECT
	Q_PROPERTY(int currentNavigationIndex READ GetCurrentNavigationIndex WRITE SetCurrentNavigationIndex NOTIFY CurrentNavigationIndexChanged)
	Q_PROPERTY(bool running READ GetRunning NOTIFY RunningChanged)
public:
	explicit GuiController(const std::string & databaseName, QObject * parent = nullptr);
	~GuiController() override;

	void Start();

public:
	Q_INVOKABLE QAbstractItemModel * GetAuthorsModel();
	Q_INVOKABLE void OnKeyPressed(int key, int modifiers);

signals:
	void CurrentNavigationIndexChanged() const;
	void RunningChanged() const;

private:
	int GetCurrentNavigationIndex() const;
	void SetCurrentNavigationIndex(int);

	bool GetRunning() const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
