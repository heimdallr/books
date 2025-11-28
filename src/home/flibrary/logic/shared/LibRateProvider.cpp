#include "LibRateProvider.h"

#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QPalette>
#include <QString>

#include "fnd/IsOneOf.h"
#include "fnd/ScopedCall.h"
#include "fnd/linear.h"
#include "fnd/try.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/SettingsConstant.h"

#include "Constant.h"
#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

std::unordered_map<long long, double> ReadRates(const ISettings& settings, const ICollectionProvider& collectionProvider, const IDatabaseUser& databaseUser)
{
	if (!collectionProvider.ActiveCollectionExists())
		return {};

	if (settings.Get(Constant::Settings::LIBRATE_VIEW_PRECISION_KEY, Constant::Settings::LIBRATE_VIEW_PRECISION_DEFAULT) <= Constant::Settings::LIBRATE_VIEW_PRECISION_DEFAULT)
		return {};

	const auto additionalFileName =
		collectionProvider.GetActiveCollection().GetFolder() + "/" + QString::fromStdWString(Inpx::REVIEWS_FOLDER) + "/" + QString::fromStdWString(Inpx::REVIEWS_ADDITIONAL_ARCHIVE_NAME);

	const auto zip = TRY(QString("open %1").arg(additionalFileName), [&] {
		return std::make_unique<Zip>(additionalFileName);
	});

	if (!zip)
		return {};

	QJsonParseError jsonParseError;
	const auto      doc = QJsonDocument::fromJson(zip->Read(Inpx::REVIEWS_ADDITIONAL_BOOKS_FILE_NAME)->GetStream().readAll(), &jsonParseError);
	if (jsonParseError.error != QJsonParseError::NoError)
	{
		PLOGW << jsonParseError.errorString();
		return {};
	}

	if (!doc.isArray())
	{
		PLOGW << "additional books.json must be an array";
		return {};
	}

	std::unordered_map<QString, double> additionalRates;
	std::ranges::transform(
		doc.array() | std::views::transform([](const auto& item) {
			auto obj = item.toObject();
			return std::make_tuple(QString("%1#%2").arg(obj["folder"].toString(), obj["file"].toString()), obj["sum"].toDouble(0.0), obj["count"].toInt(0));
		}) | std::views::filter([](const auto& item) {
			return std::get<1>(item) > 0.0 && std ::get<2>(item) > 0;
		}),
		std::inserter(additionalRates, additionalRates.end()),
		[](const auto& item) {
		return std::make_pair(std::get<0>(item), std::get<1>(item) / std::get<2>(item));
	});

	const auto db = databaseUser.Database();
	const auto query = db->CreateQuery("select b.BookID, f.FolderTitle||'#'||b.FileName from Books_View b join Folders f on f.FolderID = b.FolderID");
	std::unordered_map<long long, double> rate;
	for (query->Execute(); !query->Eof(); query->Next())
		if (const auto it = additionalRates.find(query->Get<const char*>(1)); it != additionalRates.end())
			rate.try_emplace(query->Get<long long>(0), it->second);

	return rate;
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

double LibRateProviderSimple::GetLibRate(long long /*bookId*/, const QString& libRate) const
{
	return libRate.toDouble();
}

QVariant LibRateProviderSimple::GetLibRateString(long long /*bookId*/, const QString& libRate) const
{
	return libRate.isEmpty() ? QVariant {} : QVariant { libRate };
}

QVariant LibRateProviderSimple::GetForegroundBrush(long long /*bookId*/, const QString& /*libRate*/) const
{
	return {};
}

struct LibRateProviderDouble::Impl
{
	const std::unordered_map<long long, double> rate;
	const std::map<double, uint32_t>            colors;
	const int                                   precision;
	const int                                   power { GetPower(precision) };

	Impl(const ISettings& settings, const ICollectionProvider& collectionProvider, const IDatabaseUser& databaseUser)
		: rate { ReadRates(settings, collectionProvider, databaseUser) }
		, colors { ReadColors(settings) }
		, precision { settings.Get(Constant::Settings::LIBRATE_VIEW_PRECISION_KEY, 0) }
	{
	}
};

LibRateProviderDouble::LibRateProviderDouble(
	const std::shared_ptr<const ISettings>&           settings,
	const std::shared_ptr<const ICollectionProvider>& collectionProvider,
	const std::shared_ptr<const IDatabaseUser>&       databaseUser
)
	: m_impl(*settings, *collectionProvider, *databaseUser)
{
}

LibRateProviderDouble::~LibRateProviderDouble() = default;

double LibRateProviderDouble::GetLibRate(const long long bookId, const QString& libRate) const
{
	const auto it = m_impl->rate.find(bookId);
	return it != m_impl->rate.end() ? it->second : libRate.isEmpty() ? 0.0 : libRate.toDouble();
}

QVariant LibRateProviderDouble::GetLibRateString(const long long bookId, const QString& libRate) const
{
	const auto rateValue = GetLibRate(bookId, libRate);
	return rateValue <= std::numeric_limits<double>::epsilon() ? QVariant {} : QVariant { QString::number(rateValue, 'f', m_impl->precision) };
}

QVariant LibRateProviderDouble::GetForegroundBrush(const long long bookId, const QString& libRate) const
{
	auto rateValue = GetLibRate(bookId, libRate);
	if (rateValue < 1.0 || rateValue > 5.0)
		return {};

	rateValue = std::lround(rateValue * m_impl->power) * 1.0 / m_impl->power;

	const auto& colors = m_impl->colors;
	const auto  it     = colors.upper_bound(rateValue);
	assert(!IsOneOf(it, colors.begin(), colors.end()));

	const auto get = [&](const int bits) {
		const auto   prev = std::prev(it);
		const Linear l(prev->first, static_cast<float>((prev->second >> bits) & 0xFF), it->first, static_cast<float>((it->second >> bits) & 0xFF));
		return static_cast<uint32_t>(l(rateValue)) << bits;
	};

	return QBrush { get(0) | get(8) | get(16) };
}
