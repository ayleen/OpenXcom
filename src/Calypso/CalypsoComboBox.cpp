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
#include "CalypsoHdUiOverlay.h"

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
	int maxOptions, int minPanelWidth, int panelX, int panelY, int panelW,
	int panelH, int listX, int listY, int listW,
	const std::vector<int>& rowBoundaries, bool projectedDescriptor)
{
	if (!_window || !_list) return;
	const int stride = std::max(1, rowHeight);
	const std::size_t count = _list->getTexts();
	const int insetX = std::max(0, optionInsetX);
	const int insetY = std::max(0, optionInsetY);
	const int safeW = std::max(1, boundsW);
	const int safeH = std::max(1, boundsH);
	const int safeMinPanelWidth = std::max(1, minPanelWidth);
	auto layout = Calypso::calypsoHdResolveChooserLayout(
		{getX(), getY(), getWidth(), getHeight()},
		{boundsX, boundsY, safeW, safeH}, stride, insetY,
		static_cast<int>(count), maxOptions, _popupAboveButton,
		safeMinPanelWidth);
	if (panelX >= 0 && panelY >= 0 && panelW > 0 && panelH > 0)
	{
		layout.panel.x = panelX;
		layout.panel.y = panelY;
		layout.panel.w = panelW;
		layout.panel.h = panelH;
	}
	const bool emptyDescriptor = projectedDescriptor && count == 0 && rowBoundaries.empty();
	const bool validBoundaries = emptyDescriptor || (rowBoundaries.size() >= 2
		&& rowBoundaries.front() == 0
		&& std::is_sorted(rowBoundaries.begin(), rowBoundaries.end())
		&& rowBoundaries.back() > 0);
	const std::size_t descriptorRows = emptyDescriptor ? 0
		: (validBoundaries ? rowBoundaries.size() - 1 : 0);
	const int panelWidth = layout.panel.w;
	const int descriptorListWidth = listW > 0 ? listW : std::max(1, panelWidth - 2 * insetX);
	const int descriptorListHeight = emptyDescriptor ? 0
		: (validBoundaries ? rowBoundaries.back() : 0);
	const int descriptorListX = listX >= 0 ? listX : layout.panel.x + insetX;
	const int popupY = layout.panel.y;
	const int descriptorListY = listY >= 0 ? listY : popupY + insetY;
	const bool descriptorGeometry = emptyDescriptor || (validBoundaries
		&& descriptorListWidth > 0 && descriptorListHeight > 0
		&& descriptorListX >= layout.panel.x
		&& descriptorListY >= popupY
		&& descriptorListX + descriptorListWidth <= layout.panel.x + panelWidth
		&& descriptorListY + descriptorListHeight <= popupY + layout.panel.h);
	if (projectedDescriptor)
	{
		_calypsoHdDescriptorFailed = !descriptorGeometry;
		_calypsoHdDescriptorEmpty = emptyDescriptor;
	}
	if (projectedDescriptor && !descriptorGeometry)
	{
		// An explicitly supplied HD descriptor is authoritative. Never silently
		// fall back to a rounded legacy stride: that would expose painted rows
		// whose native owner no longer matches the visible geometry.
		_list->clearCalypsoHdSelectionList();
		_window->setVisible(false);
		_list->setVisible(false);
		Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
			"invalid explicit HD chooser geometry descriptor");
		return;
	}
	if (projectedDescriptor && descriptorGeometry)
		_calypsoHdDescriptorFailed = false;
	const std::size_t visibleRows = descriptorGeometry
		? descriptorRows : static_cast<std::size_t>(std::max(0, layout.visibleRows));
	const int listWidth = descriptorGeometry ? descriptorListWidth : std::max(1, panelWidth - 2 * insetX);
	const int listHeight = descriptorGeometry ? descriptorListHeight : static_cast<int>(visibleRows) * stride;
	_window->setX(layout.panel.x);
	_window->setWidth(panelWidth);
	_window->setY(popupY);
	_window->setHeight(layout.panel.h);
	_list->setX(descriptorGeometry ? descriptorListX : layout.panel.x + insetX);
	_list->setWidth(listWidth);
	_list->rebaseNativeSize(listWidth, stride);
	_list->setMinimumRowHeight(stride);
	// The HD row rectangles already include the authored inset. The legacy
	// three-pixel margin would shift native hit testing below the painted row.
	// Fallback remains the legacy geometry: _list->setY(popupY + insetY).
	_list->setY(descriptorGeometry ? descriptorListY : popupY + insetY);
	_list->setHeight(listHeight);
	if (emptyDescriptor)
	{
		_window->setVisible(false);
		_list->setVisible(false);
		_state->clearModal(this);
	}
	_list->configureCalypsoHdSelectionList(0, 0, stride, 0, listHeight,
		visibleRows, descriptorGeometry ? rowBoundaries : std::vector<int>{});
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
