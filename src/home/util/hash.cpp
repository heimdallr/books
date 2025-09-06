#include "hash.h"

#include <QCryptographicHash>

namespace HomeCompa::Util
{

QString md5(const QByteArray& data)
{
	QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
	hash.addData(data);
	return hash.result().toHex();
}

QString GetSaltedHash(const QString& auth)
{
	auto id = QSysInfo::machineUniqueId();
	id.append(auth.toUtf8());
	return md5(id).toLower();
}

QString GetSaltedHash(const QString& user, const QString& password)
{
	return GetSaltedHash(QString("%1:%2").arg(user, password));
}

}
