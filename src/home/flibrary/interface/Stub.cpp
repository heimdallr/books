#include <ranges>

#include <QFileInfo>
#include <QLineEdit>
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

	return str.simplified();
}

}

bool IScriptController::HasMacro(const QString & str, const Macro macro)
{
	return str.contains(GetMacro(macro));
}

QString & IScriptController::SetMacro(QString & str, const Macro macro, const QString & value)
{
	const QString macroStr = GetMacro(macro);
	const auto start = str.indexOf(macroStr, 0, Qt::CaseInsensitive);
	if (start < 0)
		return str;

	const auto replace = [&] (const QString & s, const qsizetype startPos, const qsizetype endPos) -> QString&
	{
		str.erase(std::next(str.begin(), startPos), std::next(str.begin(), endPos));
		return str.insert(startPos, s);
	};

	if (start == 0 || str[start - 1] != '[')
		return replace(value, start, start + macroStr.length());

	const auto itEnd = std::find_if(std::next(str.cbegin(), start), str.cend(), [n = 1] (const QChar ch) mutable
	{
		if (ch == '[')
			++n;

		return ch == ']' && --n == 0;
	});

	if (itEnd == str.cend())
		return replace(value, start, start + macroStr.length());

	if (value.isEmpty())
		return replace(value, start - 1, std::distance(str.cbegin(), itEnd) + 1);

	str.erase(itEnd);
	str.erase(std::next(str.begin(), start) - 1);
	return replace(value, start - 1, start + macroStr.length() - 1);
}

const char * IScriptController::GetMacro(const Macro macro)
{
	return FindSecond(s_commandMacros, macro);
}

void IScriptController::SetMacroActions(QLineEdit * lineEdit)
{
	for (const auto & item : s_commandMacros | std::views::values)
	{
		const auto menuItemTitle = QString("%1\t%2").arg(Loc::Tr(s_context, item), item);
		lineEdit->addAction(menuItemTitle, [=, value = QString(item)]
		{
			auto currentText = lineEdit->text();
			const auto currentPosition = lineEdit->cursorPosition();
			lineEdit->setText(currentText.insert(currentPosition, value));
			lineEdit->setCursorPosition(currentPosition + static_cast<int>(value.size()));
		});
	}
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
