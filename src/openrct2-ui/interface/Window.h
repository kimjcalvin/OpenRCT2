/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <openrct2/interface/Window.h>
#include <openrct2/interface/Window_internal.h>

struct Window : rct_window
{
    virtual bool IsLegacy() override;
    virtual void OnDraw(rct_drawpixelinfo& dpi) override;
    virtual void OnDrawWidget(rct_widgetindex widgetIndex, rct_drawpixelinfo& dpi) override;

    void InvalidateWidget(rct_widgetindex widgetIndex);
    bool IsWidgetDisabled(rct_widgetindex widgetIndex) const;
    bool IsWidgetPressed(rct_widgetindex widgetIndex) const;
    void SetWidgetDisabled(rct_widgetindex widgetIndex, bool value);
    void SetWidgetPressed(rct_widgetindex widgetIndex, bool value);
    void SetCheckboxValue(rct_widgetindex widgetIndex, bool value);
    void DrawWidgets(rct_drawpixelinfo& dpi);
    void Close();
};

void WindowAllWheelInput();
void ApplyScreenSaverLockSetting();
