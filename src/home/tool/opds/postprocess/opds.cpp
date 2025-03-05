#include <QIODevice>

namespace HomeCompa::Opds
{

QByteArray PostProcess_opds(QIODevice& stream)
{
	auto result = stream.readAll();
	return result;
}

}
