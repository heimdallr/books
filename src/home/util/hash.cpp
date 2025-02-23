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

}
