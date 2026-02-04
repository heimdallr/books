#pragma once

#include <vector>

#include <QString>

#include "interface/logic/ICollectionProvider.h"

namespace HomeCompa
{

class ISettings;

}

namespace HomeCompa::Flibrary
{

struct CollectionImpl : Collection
{
	CollectionImpl() = default;
	CollectionImpl(QString name, QString database, QString folder, QString inpx);

	static void        Serialize(const Collection& collection, ISettings& settings);
	static Collections Deserialize(ISettings& settings);
	static Ptr         Deserialize(const ISettings& settings, QString collectionId);

	static QString GetActive(const ISettings& settings);
	static void    SetActive(ISettings& settings, const QString& uid);

	static void Remove(ISettings& settings, const QString& uid);
};

}
