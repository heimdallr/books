#include <QIODevice>

#include "fnd/IsOneOf.h"

#include "interface/logic/IAnnotationController.h"

#include "util/localization.h"

#include "root.h"

namespace HomeCompa::Opds
{

using namespace Flibrary;

namespace
{

class AnnotationControllerStrategy final : public IAnnotationController::IStrategy
{
private: // IAnnotationController::IUrlGenerator
	QString GenerateUrl(const char* /*type*/, const QString& /*id*/, const QString& str, bool) const override
	{
		return str;
	}

	QString GenerateStars(const int rate) const override
	{
		return rate > 0 && rate <= 5 ? QString::number(rate) : QString {};
	}

	void Add(const Section section, QString& text, const QString& str, const char* pattern) const override
	{
		if (str.isEmpty() || IsOneOf(section, Section::Title))
			return;

		IStrategy::Add(section, text, str, pattern);
		text.append("<br/>\n");
	}

	QString AddTableRow(const char* name, const QString& value) const override
	{
		return QString("<p>%1 %2</p>").arg(Loc::Tr("Annotation", name)).arg(value);
	}

	QString AddTableRow(const QStringList& values) const override
	{
		return QString("<p>%1</p>").arg(values.join(' '));
	}

	QString TableRowsToString(const QStringList& values) const override
	{
		return values.join('\n').append('\n');
	}
};

} // namespace

QByteArray PostProcess_opds(const IPostProcessCallback&, QIODevice& stream, ContentType, const QStringList&, const ISettings&)
{
	auto result = stream.readAll();
	return result;
}

std::unique_ptr<IAnnotationController::IStrategy> CreateAnnotationControllerStrategy_opds(const ISettings&)
{
	return std::make_unique<AnnotationControllerStrategy>();
}

} // namespace HomeCompa::Opds
