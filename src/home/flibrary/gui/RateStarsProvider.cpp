#include "RateStarsProvider.h"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

#include "interface/constants/SettingsConstant.h"
#include "util/ISettings.h"
#include "util/ISettingsObserver.h"
#include "util/serializer/Font.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto TRANSPARENT = "transparent";
constexpr auto COLOR = "#CCCC00";

QString ReadContent()
{
	QFile file(":/icons/stars.svg");
	[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
	assert(ok);
	return QString::fromUtf8(file.readAll());
}

}

class RateStarsProvider::Impl : public ISettingsObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(std::shared_ptr<ISettings> settings)
		: m_settings { std::move(settings) }
		, m_starsFileContent { ReadContent() }
	{
		m_settings->RegisterObserver(this);
		ReadFont();
	}

	~Impl() override
	{
		m_settings->UnregisterObserver(this);
	}

public:
	QPixmap GetStars(const size_t rate)
	{
		if (rate >= std::size(m_renderers))
			return {};

		if (!m_renderers[rate])
			Init(rate);

		return Render(rate);
	}

	QSize GetSize(const size_t rate)
	{
		if (rate >= std::size(m_renderers))
			return {};

		if (!m_renderers[rate])
			Init(rate);

		return m_renderers[rate]->defaultSize();
	}

	void Render(QPainter * painter, const QRect & rect, const size_t rate)
	{
		if (rate >= std::size(m_renderers))
			return;

		if (!m_renderers[rate])
			Init(rate);

		m_renderers[rate]->render(painter, rect);
	}

private: // ISettingsObserver
	void HandleValueChanged(const QString & key, const QVariant &) override
	{
		if (key == QString("%1/pointSizeF").arg(Constant::Settings::FONT_KEY))
			ReadFont();
	}

private:
	void Init(const size_t rate)
	{
		const char * colors[5];
		for (size_t i = 0; i <= rate; ++i)
			colors[i] = COLOR;
		for (size_t i = rate + 1; i < 5; ++i)
			colors[i] = TRANSPARENT;
		const auto content = m_starsFileContent.arg(colors[0]).arg(colors[1]).arg(colors[2]).arg(colors[3]).arg(colors[4]);

		m_renderers[rate] = std::make_unique<QSvgRenderer>(content.toUtf8());
		assert(m_renderers[rate]->isValid());
	}

	void ReadFont()
	{
		const SettingsGroup group(*m_settings, Constant::Settings::FONT_KEY);
		QFont font;
		Util::Deserialize(font, *m_settings);
		const QFontMetrics fontMetrics(font);
		m_height = fontMetrics.ascent();
	}

	QPixmap Render(const size_t rate) const
	{
		const auto defaultSize = m_renderers[rate]->defaultSize();
		QPixmap stars(m_height * defaultSize.width() / defaultSize.height(), m_height);
		stars.fill(Qt::transparent);
		QPainter p(&stars);
		m_renderers[rate]->render(&p, QRect(QPoint {}, stars.size()));

		return stars;
	}

private:
	int m_height;
	std::shared_ptr<ISettings> m_settings;
	QString m_starsFileContent;

	std::vector<std::unique_ptr<QSvgRenderer>> m_renderers { 5 };
};

RateStarsProvider::RateStarsProvider(std::shared_ptr<ISettings> settings)
	: m_impl(std::make_unique<Impl>(std::move(settings)))
{
}

RateStarsProvider::~RateStarsProvider() = default;

QPixmap RateStarsProvider::GetStars(const int rate) const
{
	return m_impl->GetStars(static_cast<size_t>(rate - 1));
}

QSize RateStarsProvider::GetSize(const int rate) const
{
	return m_impl->GetSize(static_cast<size_t>(rate - 1));
}

void RateStarsProvider::Render(QPainter * painter, const QRect & rect, const int rate) const
{
	m_impl->Render(painter, rect, static_cast<size_t>(rate - 1));
}
