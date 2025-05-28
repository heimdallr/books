#include "LibRateProvider.h"

#include <QFile>
#include <QJsonObject>
#include <QString>

#include "interface/constants/SettingsConstant.h"

#include "inpx/src/util/constant.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

std::unordered_map<QString, double> ReadRate(const ISettings& settings, const ICollectionProvider& collectionProvider)
{
	if (!collectionProvider.ActiveCollectionExists())
		return {};

	if (settings.Get(Constant::Settings::STAR_VIEW_PRECISION, Constant::Settings::STAR_VIEW_PRECISION_DEFAULT) <= Constant::Settings::STAR_VIEW_PRECISION_DEFAULT)
		return {};

	const auto additionalFileName = collectionProvider.GetActiveCollection().folder + "/" + QString::fromStdWString(REVIEWS_FOLDER) + "/" + REVIEWS_ADDITIONAL_ARCHIVE_NAME;
	if (!QFile::exists(additionalFileName))
		return {};
	try
	{
		const Zip zip(additionalFileName);
		QJsonParseError jsonParseError;
		const auto doc = QJsonDocument::fromJson(zip.Read(REVIEWS_ADDITIONAL_BOOKS_FILE_NAME)->GetStream().readAll(), &jsonParseError);
		if (jsonParseError.error != QJsonParseError::NoError)
		{
			PLOGW << jsonParseError.errorString();
			return {};
		}

		const auto obj = doc.object();
		std::unordered_map<QString, double> rate;
		for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
			rate.emplace(it.key(), it.value().toObject()["libRate"].toDouble());

		return rate;
	}
	catch (const std::exception& ex)
	{
		PLOGW << ex.what();
	}
	catch (...)
	{
		PLOGW << "unknown error";
	}

	return {};
}

} // namespace

QString LibRateProviderSimple::GetLibRate(const QString& /*libId*/, QString libRate) const
{
	return libRate;
}

LibRateProviderDouble::LibRateProviderDouble(const std::shared_ptr<const ISettings>& settings, const std::shared_ptr<const ICollectionProvider>& collectionProvider)
	: m_rate { ReadRate(*settings, *collectionProvider) }
	, m_precision { settings->Get(Constant::Settings::STAR_VIEW_PRECISION, 0) }
{
}

QString LibRateProviderDouble::GetLibRate(const QString& libId, QString libRate) const
{
	const auto get = [this](const double rate) { return rate <= std::numeric_limits<double>::epsilon() ? QString {} : QString::number(rate, 'f', m_precision); };
	const auto it = m_rate.find(libId);
	return it != m_rate.end() ? get(it->second) : libRate.isEmpty() ? libRate : get(libRate.toDouble());
}
