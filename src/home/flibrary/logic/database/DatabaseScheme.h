#pragma once

namespace HomeCompa::DB
{
class IDatabase;
}

namespace HomeCompa::Flibrary
{
class ICollectionProvider;
}

namespace HomeCompa::Flibrary::DatabaseScheme
{
void Update(DB::IDatabase& db, const ICollectionProvider& collectionProvider);
}
