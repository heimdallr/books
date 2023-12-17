#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>

#include "fnd/FindPair.h"

#include "constants/Localization.h"
#include "constants/ModelRole.h"

#include "logic/ILogicFactory.h"
#include "logic/IScriptController.h"

namespace HomeCompa::Flibrary::Loc {

QString Tr(const char * context, const char * str)
{
	return QCoreApplication::translate(context, str);
}

}

namespace HomeCompa::Flibrary {

namespace {

QString RemoveIllegalCharacters(QString str)
{
	str.remove(QRegularExpression(R"([/\\<>:"\|\?\*\t])"));

	while (!str.isEmpty() && str.endsWith('.'))
		str.chop(1);

	if (str.isEmpty())
		str = "_";

	return str.simplified();
}

}

bool IScriptController::HasMacro(const QString & str, const Macro macro)
{
	return str.contains(GetMacro(macro));
}

QString & IScriptController::SetMacro(QString & str, const Macro macro, const QString & value)
{
	return str.replace(GetMacro(macro), value);
}

const char * IScriptController::GetMacro(const Macro macro)
{
	return FindSecond(s_commandMacros, macro);
}

std::vector<std::vector<QString>> ILogicFactory::GetSelectedBookIds(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, const std::vector<int> & roles)
{
	QModelIndexList selected;
	model->setData({}, QVariant::fromValue(SelectedRequest { index, indexList, &selected }), Role::Selected);

	std::vector<std::vector<QString>> result;
	result.reserve(selected.size());
	std::ranges::transform(selected, std::back_inserter(result), [&] (const auto & selectedIndex)
	{
		std::vector<QString> resultItem;
		std::ranges::transform(roles, std::back_inserter(resultItem), [&] (const int role)
		{
			return selectedIndex.data(role).toString();
		});
		return resultItem;
	});

	return result;
}

ILogicFactory::ExtractedBooks ILogicFactory::GetExtractedBooks(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList)
{
	ExtractedBooks books;

	const std::vector<int> roles { Role::Folder, Role::FileName, Role::Size, Role::AuthorFull, Role::Series, Role::SeqNumber, Role::Title };
	const auto selected = GetSelectedBookIds(model, index, indexList, roles);
	std::ranges::transform(selected, std::back_inserter(books), [&] (auto && book)
	{
		assert(book.size() == roles.size());
		return ExtractedBook { std::move(book[0]), std::move(book[1]), book[2].toLongLong(), std::move(book[3]), std::move(book[4]), book[5].toInt(), std::move(book[6]) };
	});

	return books;
}

void ILogicFactory::FillScriptTemplate(QString & scriptTemplate, const ExtractedBook & book)
{
	const QFileInfo fileInfo(book.file);
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::Title, RemoveIllegalCharacters(book.title));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::FileExt, RemoveIllegalCharacters(fileInfo.suffix()));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::FileName, RemoveIllegalCharacters(fileInfo.fileName()));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::BaseFileName, RemoveIllegalCharacters(fileInfo.completeBaseName()));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::Uid, QUuid::createUuid().toString(QUuid::WithoutBraces));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::Author, RemoveIllegalCharacters(book.author));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::Series, RemoveIllegalCharacters(book.series));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::SeqNumber, book.seqNumber > 0 ? QString::number(book.seqNumber) : QString {});
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::FileSize, QString::number(book.size));
}

}
