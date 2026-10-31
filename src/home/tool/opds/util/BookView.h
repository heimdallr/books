#pragma once

namespace HomeCompa::Flibrary {

class IFilterProvider;

}

namespace HomeCompa {

class ISettings;

}

namespace HomeCompa::DB {

class IDatabaseWritable;

}

namespace HomeCompa::Opds::BookView {

void Create(DB::IDatabaseWritable& db, const ISettings& settings, const Flibrary::IFilterProvider& filterProvider);

}
