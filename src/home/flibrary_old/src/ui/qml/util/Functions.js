function SetWidths(items, parent, fillWidthSetter)
{
	var i, length
	for (i = 0, length = items.length; i < length; ++i)
	{
		fillWidthSetter(items[i], false)
		items[i].parent = null
		items[i].ready = false
	}

	items.sort(function (i, b) { return i.getIndex() - b.getIndex(); })

	for (i = 0, length = items.length; i < length; ++i)
		items[i].parent = parent

	var firstIndex = -1
	for (i = 0, length = items.length; i < length; ++i)
	{
		if (items[i].visible)
		{
			if (firstIndex < 0)
				firstIndex = i
			else
				items[i].ready = true
		}
	}

	if (firstIndex >= 0)
		fillWidthSetter(items[firstIndex], true)
}

function GetMargin()
{
	return uiSettings.heightRow / 10
}