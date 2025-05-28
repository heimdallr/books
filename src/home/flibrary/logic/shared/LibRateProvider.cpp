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

	if (settings.Get(Constant::Settings::LIBRATE_VIEW_PRECISION_KEY, Constant::Settings::LIBRATE_VIEW_PRECISION_DEFAULT) <= Constant::Settings::LIBRATE_VIEW_PRECISION_DEFAULT)
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

QVariant LibRateProviderSimple::GetLibRate(const QString& /*libId*/, const QString& libRate) const
{
	return libRate;
}

LibRateProviderDouble::LibRateProviderDouble(const std::shared_ptr<const ISettings>& settings, const std::shared_ptr<const ICollectionProvider>& collectionProvider)
	: m_rate { ReadRate(*settings, *collectionProvider) }
	, m_precision { settings->Get(Constant::Settings::LIBRATE_VIEW_PRECISION_KEY, 0) }
{
}

QVariant LibRateProviderDouble::GetLibRate(const QString& libId, const QString& libRate) const
{
	const auto rateValue = GetRateValue(libId, libRate);
	return rateValue <= std::numeric_limits<double>::epsilon() ? QString {} : QString::number(rateValue, 'f', m_precision);
}

double LibRateProviderDouble::GetRateValue(const QString& libId, const QString& libRate) const
{
	const auto it = m_rate.find(libId);
	return it != m_rate.end() ? it->second : libRate.isEmpty() ? 0.0 : libRate.toDouble();
}
