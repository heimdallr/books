#pragma once

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/ILibRateProvider.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class AbstractLibRateProvider : virtual public ILibRateProvider
{
};

class LibRateProviderSimple : public AbstractLibRateProvider
{
private: // ILibRateProvider
	QVariant GetLibRate(const QString& libId, const QString& libRate) const override;
	QVariant GetForegroundBrush(const QString& libId, const QString& libRate) const override;
};

class LibRateProviderDouble : public AbstractLibRateProvider
{
public:
	LibRateProviderDouble(const std::shared_ptr<const ISettings>& settings, const std::shared_ptr<const ICollectionProvider>& collectionProvider);

private: // ILibRateProvider
	QVariant GetLibRate(const QString& libId, const QString& libRate) const override;
	QVariant GetForegroundBrush(const QString& libId, const QString& libRate) const override;

private:
	double GetRateValue(const QString& libId, const QString& libRate) const;

private:
	const std::unordered_map<QString, double> m_rate;
	const int m_precision;
};

} // namespace HomeCompa::Flibrary
