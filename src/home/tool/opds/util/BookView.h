#pragma once

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

void Create(DB::IDatabase& db, const ISettings& settings);

}
