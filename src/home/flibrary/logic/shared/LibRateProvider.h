#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

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
	double   GetLibRate(const QString& libId, const QString& libRate) const override;
	QVariant GetLibRateString(const QString& libId, const QString& libRate) const override;
	QVariant GetForegroundBrush(const QString& libId, const QString& libRate) const override;
};

class LibRateProviderDouble : public AbstractLibRateProvider
{
	NON_COPY_MOVABLE(LibRateProviderDouble)

public:
	LibRateProviderDouble(const std::shared_ptr<const ISettings>& settings, const std::shared_ptr<const ICollectionProvider>& collectionProvider);
	~LibRateProviderDouble() override;

private: // ILibRateProvider
	double   GetLibRate(const QString& libId, const QString& libRate) const override;
	QVariant GetLibRateString(const QString& libId, const QString& libRate) const override;
	QVariant GetForegroundBrush(const QString& libId, const QString& libRate) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
