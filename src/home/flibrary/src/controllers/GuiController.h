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
public:
	explicit GuiController(const std::string & databaseName, QObject * parent = nullptr);
	~GuiController() override;

	void Start();

public:
	Q_INVOKABLE QAbstractItemModel * GetAuthorsModel();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
