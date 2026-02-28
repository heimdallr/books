#pragma once

namespace HomeCompa::Flibrary
{

class IFilterProvider;

}

namespace HomeCompa
{

class ISettings;

}

namespace HomeCompa::DB
{

class IDatabase;

}

namespace HomeCompa::Opds::BookView
{

void Create(DB::IDatabase& db, const ISettings& settings, const Flibrary::IFilterProvider& filterProvider);

}
