#include <QIODevice>

#include "interface/logic/IAnnotationController.h"

#include "root.h"

namespace HomeCompa::Opds
{

using namespace Flibrary;

namespace
{

class AnnotationControllerStrategy final : public IAnnotationController::IStrategy
{
private: // IAnnotationController::IUrlGenerator
	QString GenerateUrl(const char* /*type*/, const QString& /*id*/, const QString& str) const override
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

std::unique_ptr<IAnnotationController::IStrategy> CreateAnnotationControllerStrategy_opds()
{
	return std::make_unique<AnnotationControllerStrategy>();
}

} // namespace HomeCompa::Opds
