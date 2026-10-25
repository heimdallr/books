#include "TreeViewDelegateBooks.h"

#include <array>
#include <ranges>

#include <QApplication>
#include <QHeaderView>
#include <QMetaEnum>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeView>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"
#include "fnd/ValueGuard.h"
#include "fnd/observable.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/ui/IUiFactory.h"

#include "logic/data/DataItem.h"
#include "settings/ISettings.h"

#include "Measure.h"
#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto COLUMN_ALIGNMENT = "Preferences/Books/Alignment/%1";

class ITextDelegate // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ITextDelegate()                                             = default;
	[[nodiscard]] virtual QString GetString(const QVariant& value) const = 0;
};

class TextDelegateDefault final : public ITextDelegate
{
private: // ITextDelegate
	[[nodiscard]] QString GetString(const QVariant& value) const override
	{
		return value.toString();
	}
};

class TextDelegateStars final : public ITextDelegate
{
public:
	explicit TextDelegateStars(const ISettings& settings, const QString& key)
		: m_star { settings.Get(key, Constant::Settings::STAR_SYMBOL_DEFAULT) }
		, m_zero { settings.Get(Constant::Settings::PREFER_USER_RATE_ZERO_SYMBOL_KEY, Constant::Settings::STAR_SYMBOL_DEFAULT) }
	{
	}

private: // ITextDelegate
	[[nodiscard]] QString GetString(const QVariant& value) const override
	{
		bool       ok   = false;
		const auto rate = value.toInt(&ok);
		if (!ok)
			return {};

		return rate == 0 ? QString { m_zero } : rate < 0 || rate > 5 ? QString {} : QString(rate, m_star);
	}

private:
	const QChar m_star;
	const QChar m_zero;
};

class TextDelegateSize final : public ITextDelegate
{
private: // ITextDelegate
	[[nodiscard]] QString GetString(const QVariant& value) const override
	{
		bool       ok     = false;
		const auto result = value.toULongLong(&ok);
		return ok && result > 0 ? Measure::GetSize(result) : QString {};
	}
};

class TextDelegateNumber final : public ITextDelegate
{
private: // ITextDelegate
	[[nodiscard]] QString GetString(const QVariant& value) const override
	{
		bool       ok     = false;
		const auto result = value.toInt(&ok);
		return ok && result > 0 ? QString::number(result) : QString {};
	}
};

void SetAlignment(Qt::Alignment& alignment, const ISettings& settings, const QString& columnName)
{
	const auto alignmentVar = settings.Get(QString(COLUMN_ALIGNMENT).arg(columnName));
	if (!alignmentVar.isValid())
		return;

	bool       ok    = false;
	const auto value = QMetaEnum::fromType<Qt::Alignment>().keyToValue(alignmentVar.toByteArray().data(), &ok);
	if (ok)
		alignment = static_cast<Qt::Alignment>(value);
}

} // namespace

class TreeViewDelegateBooks::Impl final
    : public QStyledItemDelegate
    , public Observable<ITreeViewDelegate::IObserver>
{
public:
	Impl(const IUiFactory& uiFactory, std::shared_ptr<const ISettings> settings)
		: m_view { uiFactory.GetTreeView() }
		, m_settings { std::move(settings) }
	{
		m_alignments.fill(Qt::AlignLeft);

		for (const auto column : { BookItem::Column::Size, BookItem::Column::SeqNumber, BookItem::Column::Year })
			m_alignments[column] = Qt::AlignRight;
		m_alignments[BookItem::Column::Lang] = Qt::AlignHCenter;

		const auto textDelegateDefault = std::make_shared<TextDelegateDefault>();

#define BOOKS_COLUMN_ITEM(NAME) SetAlignment(m_alignments[BookItem::Column::NAME], *m_settings, #NAME); m_textDelegates[BookItem::Column::NAME] = textDelegateDefault;
		BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM

		m_textDelegates[BookItem::Column::LibRate]   = std::make_shared<TextDelegateStars>(*m_settings, Constant::Settings::PREFER_LIB_RATE_STAR_SYMBOL_KEY);
		m_textDelegates[BookItem::Column::UserRate]  = std::make_shared<TextDelegateStars>(*m_settings, Constant::Settings::PREFER_USER_RATE_STAR_SYMBOL_KEY);
		m_textDelegates[BookItem::Column::Size]      = std::make_shared<TextDelegateSize>();
		m_textDelegates[BookItem::Column::SeqNumber] = m_textDelegates[BookItem::Column::Year] = std::make_shared<TextDelegateNumber>();
	}

private: // QStyledItemDelegate
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		auto o = option;
		if (index.data(Role::Type).value<ItemType>() == ItemType::Books)
			return RenderBooks(*painter, o, index);

		if (index.column() != 0)
			return;

		const auto* header = m_view.header();
		int         width  = 0;
		for (auto i = 0, sz = header->count(); i < sz; ++i)
			width += header->sectionSize(i);
		width -= o.rect.x();

		o.rect.setWidth(width);

		QStyledItemDelegate::paint(painter, o, index);
	}

	QString displayText(const QVariant& value, const QLocale& /*locale*/) const override
	{
		return m_textDelegates[m_currentColumn]->GetString(value);
	}

private:
	void RenderBooks(QPainter& painter, QStyleOptionViewItem& o, const QModelIndex& index) const
	{
		const auto column  = index.data(Role::Remap).toInt();
		o.displayAlignment = m_alignments[static_cast<size_t>(column)];
		m_currentColumn    = column;
		QStyledItemDelegate::paint(&painter, o, index);
	}

private:
	QTreeView&                                              m_view;
	const std::shared_ptr<const ISettings>                  m_settings;

	std::array<Qt::Alignment, BookItem::Column::Last>                        m_alignments {};
	std::array<std::shared_ptr<const ITextDelegate>, BookItem::Column::Last> m_textDelegates;
	mutable size_t                                                           m_currentColumn { 0 };
};

TreeViewDelegateBooks::TreeViewDelegateBooks(const std::shared_ptr<const IUiFactory>& uiFactory, std::shared_ptr<const ISettings> settings)
	: m_impl(*uiFactory, std::move(settings))
{
	PLOGV << "TreeViewDelegateBooks created";
}

TreeViewDelegateBooks::~TreeViewDelegateBooks()
{
	PLOGV << "TreeViewDelegateBooks destroyed";
}

QAbstractItemDelegate* TreeViewDelegateBooks::GetDelegate() noexcept
{
	return m_impl.get();
}

void TreeViewDelegateBooks::OnModelChanged(const QAbstractItemModel&)
{
}

void TreeViewDelegateBooks::SetEnabled(bool /*enabled*/) noexcept
{
}

void TreeViewDelegateBooks::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void TreeViewDelegateBooks::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
