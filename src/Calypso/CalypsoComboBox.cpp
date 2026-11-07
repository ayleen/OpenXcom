#ifdef __EMSCRIPTEN__

#include "../Interface/ComboBox.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Engine/Surface.h"
#include "../Engine/Language.h"
#include "../Engine/TTFFont.h"
#include "../fmath.h"
#include "CalypsoHdOperationsLayout.h"

#include <algorithm>

namespace OpenXcom
{

void ComboBox::relayout()
{
	float s = scale();
	int bw = (int)Round(BUTTON_WIDTH * s);
	int hm = (int)Round(HORIZONTAL_MARGIN * s);
	int vm = (int)Round(VERTICAL_MARGIN * s);
	_button->setWidth(getWidth());
	_button->setHeight(getHeight());
	delete _arrow;
	_arrow = new Surface((int)Round(11 * s), (int)Round(8 * s),
		getX() + getWidth() - bw, getY() + (int)Round(4 * s));
	_arrow->setPalette(getPalette());
	int listW = getWidth() - hm * 2 - bw + 1;
	_list->setWidth(listW);
	_list->setColumns(1, _nativeW - HORIZONTAL_MARGIN * 2 - BUTTON_WIDTH + 1);
	_window->setWidth(getWidth());
	setX(getX());
	setY(getY());
	drawArrow();
	if (!_optionsCache.empty())
	{
		setDropdown((int)_optionsCache.size());
		_list->clearList();
		for (const auto& option : _optionsCache)
		{
			if (_optionsCacheTranslate)
				_list->addRow(1, _lang->getString(option).c_str());
			else
				_list->addRow(1, option.c_str());
		}
		setSelected(_sel);
	}
}

void ComboBox::setWidth(int w)
{
	Surface::setWidth(w);
	relayout();
}

void ComboBox::setHeight(int h)
{
	Surface::setHeight(h);
	relayout();
}

void ComboBox::setTTFFont(TTFFont* font, float fillFrac)
{
	_button->setTTFFont(font, fillFrac);
	if (_list) _list->setTTFFont(font, fillFrac);
}

bool ComboBox::calypsoHdPopupVisible() const
{
	return _window != nullptr && _list != nullptr
		&& _window->getVisible() && _list->getVisible();
}

const TextList* ComboBox::calypsoHdPopupList() const
{
	return _list;
}

void ComboBox::calypsoHdPopupWindowRect(int& x, int& y, int& width, int& height) const
{
	if (!_window)
	{
		x = y = width = height = 0;
		return;
	}
	x = _window->getX();
	y = _window->getY();
	width = _window->getWidth();
	height = _window->getHeight();
}

void ComboBox::calypsoHdConfigurePopupSelection(int rowHeight, int optionInsetX,
	int optionInsetY, int boundsX, int boundsY, int boundsW, int boundsH,
	int maxOptions, int minPanelWidth)
{
	if (!_window || !_list) return;
	const int stride = std::max(1, rowHeight);
	const std::size_t count = _list->getTexts();
	const int insetX = std::max(0, optionInsetX);
	const int insetY = std::max(0, optionInsetY);
	const int safeW = std::max(1, boundsW);
	const int safeH = std::max(1, boundsH);
	const int safeMinPanelWidth = std::max(1, minPanelWidth);
	const auto layout = Calypso::calypsoHdResolveChooserLayout(
		{getX(), getY(), getWidth(), getHeight()},
		{boundsX, boundsY, safeW, safeH}, stride, insetY,
		static_cast<int>(count), maxOptions, _popupAboveButton,
		safeMinPanelWidth);
	const int panelWidth = layout.panel.w;
	const int listWidth = std::max(1, panelWidth - 2 * insetX);
	const int listHeight = std::max(1, layout.visibleRows * stride);
	const int popupY = layout.panel.y;
	_window->setX(layout.panel.x);
	_window->setWidth(panelWidth);
	_window->setY(popupY);
	_window->setHeight(layout.panel.h);
	_list->setX(layout.panel.x + insetX);
	_list->setWidth(listWidth);
	_list->rebaseNativeSize(listWidth, stride);
	_list->setMinimumRowHeight(stride);
	// The HD row rectangles already include the authored inset. The legacy
	// three-pixel margin would shift native hit testing below the painted row.
	_list->setY(popupY + insetY);
	_list->setHeight(listHeight);
	_list->configureCalypsoHdSelectionList(0, 0, stride, 0, listHeight,
		layout.visibleRows);
}

void ComboBox::calypsoHdSuppressPopupWidgets(std::vector<const void*>& widgets) const
{
	const void* owned[] = {this, _button, _arrow, _window, _list};
	for (const void* widget : owned)
	{
		if (widget && std::find(widgets.begin(), widgets.end(), widget) == widgets.end())
			widgets.push_back(widget);
	}
}

}

#endif
