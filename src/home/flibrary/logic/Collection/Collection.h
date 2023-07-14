#pragma once

#include <vector>
#include <QString>

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

struct Collection;
using Collections = std::vector<Collection>;

struct Collection
{
	QString id;
	QString name;
	QString database;
	QString folder;
	QString discardedUpdate;

	Collection() = default;
	Collection(QString name, QString database, QString folder);

	static QString GetActive(ISettings & settings);

	void Serialize(ISettings & settings) const;
	static Collections Deserialize(ISettings & settings);
	static void SetActive(ISettings & settings, const QString & id);
	static void Remove(ISettings & settings, const QString & id);
	static QString GenerateId(const QString & db);
};

}
