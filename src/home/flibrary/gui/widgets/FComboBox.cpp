#include "FComboBox.h"

#include <QAbstractItemView>

using namespace HomeCompa::Flibrary;

namespace
{

int GetMinimumWidth(const QComboBox& self)
{
	const QFontMetrics metrics(self.view()->font());
	int max = 0;
	for (int i = 0, sz = self.count(); i < sz; ++i)
		max = std::max(max, metrics.boundingRect(self.itemText(i)).width());
	return max + 3 * self.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
}

}

FComboBox::FComboBox(QWidget* parent)
	: QComboBox(parent)
{
}

void FComboBox::showPopup()
{
	view()->setMinimumWidth(GetMinimumWidth(*this));
	QComboBox::showPopup();
}
