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
	QString GetLibRate(const QString& libId, QString libRate) const override;
};

class LibRateProviderDouble : public AbstractLibRateProvider
{
public:
	LibRateProviderDouble(const std::shared_ptr<const ISettings>& settings, const std::shared_ptr<const ICollectionProvider>& collectionProvider);

private: // ILibRateProvider
	QString GetLibRate(const QString& libId, QString libRate) const override;

private:
	const std::unordered_map<QString, double> m_rate;
	const int m_precision;
};

} // namespace HomeCompa::Flibrary
