#pragma once

namespace HomeCompa::DB {

class IDatabaseWritable;

}

namespace HomeCompa::Flibrary {

class ICollectionProvider;

}

namespace HomeCompa::Flibrary::DatabaseScheme {

void Update(DB::IDatabaseWritable& db, const ICollectionProvider& collectionProvider);

}
