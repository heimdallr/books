#include <QIODevice>

#include "root.h"

namespace HomeCompa::Opds
{

QByteArray PostProcess_opds(QIODevice& stream, ContentType)
{
	auto result = stream.readAll();
	return result;
}

}
