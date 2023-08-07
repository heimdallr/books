#pragma once

#include <vector>
#include <QString>

#include "CollectionImpl.h"
#include "interface/logic/ICollectionController.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

struct CollectionImpl : Collection
{
	CollectionImpl() = default;
	CollectionImpl(QString name, QString database, QString folder);

	static void Serialize(const Collection & collection, ISettings & settings);
	static Collections Deserialize(ISettings & settings);

	static QString GetActive(const ISettings & settings);
	static void SetActive(ISettings & settings, const QString & uid);

	static void Remove(ISettings & settings, const QString & uid);
	static QString GenerateId(const QString & databaseFileName);
};

}
