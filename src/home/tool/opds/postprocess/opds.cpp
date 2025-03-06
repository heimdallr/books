#include <QIODevice>

#include "interface/logic/IAnnotationController.h"

#include "root.h"

namespace HomeCompa
{
class ISettings;
}

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

	QString GenerateStars(const int rate) const override
	{
		return rate > 0 && rate <= 5 ? QString::number(rate) : QString {};
	}
};

}

QByteArray PostProcess_opds(QIODevice& stream, ContentType)
{
	auto result = stream.readAll();
	return result;
}

std::unique_ptr<IAnnotationController::IStrategy> CreateAnnotationControllerStrategy_opds(const ISettings&)
{
	return std::make_unique<AnnotationControllerStrategy>();
}

} // namespace HomeCompa::Opds
