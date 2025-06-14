#include "LibRateProvider.h"

#include <QFile>
#include <QGuiApplication>
#include <QJsonObject>
#include <QPalette>
#include <QString>

#include "fnd/IsOneOf.h"
#include "fnd/ScopedCall.h"
#include "fnd/linear.h"

#include "interface/constants/SettingsConstant.h"

#include "inpx/src/util/constant.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

std::unordered_map<QString, double> ReadRates(const ISettings& settings, const ICollectionProvider& collectionProvider)
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
			rate.try_emplace(it.key(), it.value().toObject()["libRate"].toDouble());

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

std::map<double, uint32_t> ReadColors(const ISettings& settings)
{
	const auto color = QGuiApplication::palette().brush(QPalette::ColorRole::WindowText).color().rgb();

	std::map<double, uint32_t> result {
		{ 0.0, color },
		{ 6.0, color },
	};

	SettingsGroup viewColorsGroup(settings, Constant::Settings::LIBRATE_VIEW_COLORS_KEY);
	for (const auto& key : settings.GetKeys())
	{
		bool ok = false;
		if (const auto value = key.toDouble(&ok); ok)
			result.try_emplace(value, settings.Get(key, color));
	}

	return result;
}

int GetPower(const int precision)
{
	int result = 1;
	for (int i = 0; i < precision; ++i)
		result *= 10;
	return result;
}

} // namespace

double LibRateProviderSimple::GetLibRate(const QString& /*libId*/, const QString& libRate) const
{
	return libRate.toDouble();
}

QVariant LibRateProviderSimple::GetLibRateString(const QString& /*libId*/, const QString& libRate) const
{
	return libRate;
}

QVariant LibRateProviderSimple::GetForegroundBrush(const QString& /*libId*/, const QString& /*libRate*/) const
{
	return {};
}

struct LibRateProviderDouble::Impl
{
	const std::unordered_map<QString, double> rate;
	const std::map<double, uint32_t> colors;
	const int precision;
	const int power { GetPower(precision) };

	Impl(const ISettings& settings, const ICollectionProvider& collectionProvider)
		: rate { ReadRates(settings, collectionProvider) }
		, colors { ReadColors(settings) }
		, precision { settings.Get(Constant::Settings::LIBRATE_VIEW_PRECISION_KEY, 0) }
	{
	}
};

LibRateProviderDouble::LibRateProviderDouble(const std::shared_ptr<const ISettings>& settings, const std::shared_ptr<const ICollectionProvider>& collectionProvider)
	: m_impl(*settings, *collectionProvider)
{
}

LibRateProviderDouble::~LibRateProviderDouble() = default;

double LibRateProviderDouble::GetLibRate(const QString& libId, const QString& libRate) const
{
	const auto it = m_impl->rate.find(libId);
	return it != m_impl->rate.end() ? it->second : libRate.isEmpty() ? 0.0 : libRate.toDouble();
}

QVariant LibRateProviderDouble::GetLibRateString(const QString& libId, const QString& libRate) const
{
	const auto rateValue = GetLibRate(libId, libRate);
	return rateValue <= std::numeric_limits<double>::epsilon() ? QString {} : QString::number(rateValue, 'f', m_impl->precision);
}

QVariant LibRateProviderDouble::GetForegroundBrush(const QString& libId, const QString& libRate) const
{
	auto rateValue = GetLibRate(libId, libRate);
	if (rateValue < 1.0 || rateValue > 5.0)
		return {};

	rateValue = std::lround(rateValue * m_impl->power) * 1.0 / m_impl->power;

	const auto& colors = m_impl->colors;
	const auto it = colors.upper_bound(rateValue);
	assert(!IsOneOf(it, colors.begin(), colors.end()));

	const auto get = [&](const int bits)
	{
		const auto prev = std::prev(it);
		const Linear l(prev->first, static_cast<float>((prev->second >> bits) & 0xFF), it->first, static_cast<float>((it->second >> bits) & 0xFF));
		return static_cast<uint32_t>(l(rateValue)) << bits;
	};

	return QBrush { get(0) | get(8) | get(16) };
}
