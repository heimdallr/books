#include <QIODevice>

#include "interface/logic/IAnnotationController.h"

#include "root.h"

namespace HomeCompa::Opds
{

using namespace Flibrary;

namespace
{

class UrlGenerator final : public IAnnotationController::IUrlGenerator
{
private: // IAnnotationController::IUrlGenerator
	QString Generate(const char* /*type*/, const QString& /*id*/, const QString& str) const override
	{
		return str;
	}
};

}

QByteArray PostProcess_opds(QIODevice& stream, ContentType)
{
	auto result = stream.readAll();
	return result;
}

std::unique_ptr<IAnnotationController::IUrlGenerator> CreateUrlGenerator_opds()
{
	return std::make_unique<UrlGenerator>();
}

}
