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

}

class RateStarsProvider::Impl : public ISettingsObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(std::shared_ptr<ISettings> settings)
		: m_settings(std::move(settings))
	{
		m_settings->RegisterObserver(this);
		ReadFont();
	}

	~Impl() override
	{
		m_settings->UnregisterObserver(this);
	}

public:
	QPixmap GetStars(const int rate)
	{
		if (rate < 1 || rate > static_cast<int>(std::size(m_cache)))
			return {};

		ReadContent();

		auto & stars = m_cache[rate - 1];
		Render(stars, rate);

		return stars;
	}

private: // ISettingsObserver
	void HandleValueChanged(const QString & key, const QVariant &) override
	{
		if (key == QString("%1/pointSizeF").arg(Constant::Settings::FONT_KEY))
			ReadFont();
	}

private:
	void ReadFont()
	{
		const SettingsGroup group(*m_settings, Constant::Settings::FONT_KEY);
		Util::Deserialize(m_font, *m_settings);
	}

	void ReadContent()
	{
		if (!m_starsFileContent.isEmpty())
			return;

		QFile file(":/icons/stars.svg");
		[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
		assert(ok);
		m_starsFileContent = QString::fromUtf8(file.readAll());
	}

	void Render(QPixmap & stars, const int rate) const
	{
		const QFontMetrics fontMetrics(m_font);
		const auto height = fontMetrics.ascent();
		if (!stars.isNull() && stars.height() == height)
			return;

		const char * colors[5];
		for (int i = 0; i < rate; ++i)
			colors[i] = COLOR;
		for (int i = rate; i < 5; ++i)
			colors[i] = TRANSPARENT;
		const auto content = m_starsFileContent.arg(colors[0]).arg(colors[1]).arg(colors[2]).arg(colors[3]).arg(colors[4]);

		QSvgRenderer renderer(content.toUtf8());
		assert(renderer.isValid());

		const auto defaultSize = renderer.defaultSize();
		stars = QPixmap(height * defaultSize.width() / defaultSize.height(), height);
		stars.fill(Qt::transparent);
		QPainter p(&stars);
		renderer.render(&p, QRect(QPoint {}, stars.size()));
	}

private:
	QFont m_font;
	std::shared_ptr<ISettings> m_settings;
	QPixmap m_cache[5];
	QString m_starsFileContent;
};

RateStarsProvider::RateStarsProvider(std::shared_ptr<ISettings> settings)
	: m_impl(std::make_unique<Impl>(std::move(settings)))
{
}

RateStarsProvider::~RateStarsProvider() = default;

QPixmap RateStarsProvider::GetStars(const int rate) const
{
	return m_impl->GetStars(rate);
}
