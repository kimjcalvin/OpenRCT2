/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <openrct2-ui/interface/LandTool.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Context.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/world/Park.h>

static constexpr const rct_string_id WINDOW_TITLE = STR_WATER;
static constexpr const int32_t WH = 77;
static constexpr const int32_t WW = 76;

// clang-format off
enum WINDOW_WATER_WIDGET_IDX {
    WIDX_BACKGROUND,
    WIDX_TITLE,
    WIDX_CLOSE,
    WIDX_PREVIEW,
    WIDX_DECREMENT,
    WIDX_INCREMENT
};

static rct_widget window_water_widgets[] = {
    WINDOW_SHIM(WINDOW_TITLE, WW, WH),
    MakeWidget     ({16, 17}, {44, 32}, WindowWidgetType::ImgBtn, WindowColour::Primary , SPR_LAND_TOOL_SIZE_0,   STR_NONE),                     // preview box
    MakeRemapWidget({17, 18}, {16, 16}, WindowWidgetType::TrnBtn, WindowColour::Tertiary, SPR_LAND_TOOL_DECREASE, STR_ADJUST_SMALLER_WATER_TIP), // decrement size
    MakeRemapWidget({43, 32}, {16, 16}, WindowWidgetType::TrnBtn, WindowColour::Tertiary, SPR_LAND_TOOL_INCREASE, STR_ADJUST_LARGER_WATER_TIP),  // increment size
    { WIDGETS_END },
};
// clang-format on

class WaterWindow final : public Window
{
public:
    void OnOpen() override
    {
        widgets = window_water_widgets;
        enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_DECREMENT) | (1 << WIDX_INCREMENT) | (1 << WIDX_PREVIEW);
        hold_down_widgets = (1 << WIDX_INCREMENT) | (1 << WIDX_DECREMENT);
        WindowInitScrollWidgets(this);
        window_push_others_below(this);

        gLandToolSize = 1;
        gWaterToolRaiseCost = MONEY32_UNDEFINED;
        gWaterToolLowerCost = MONEY32_UNDEFINED;
    }

    void OnClose() override
    {
        // If the tool wasn't changed, turn tool off
        if (water_tool_is_active())
        {
            tool_cancel();
        }
    }

    void OnMouseUp(rct_widgetindex widgetIndex) override
    {
        switch (widgetIndex)
        {
            case WIDX_CLOSE:
                Close();
                break;
            case WIDX_PREVIEW:
                InputSize();
                break;
        }
    }

    void OnMouseDown(rct_widgetindex widgetIndex) override
    {
        switch (widgetIndex)
        {
            case WIDX_DECREMENT:
                // Decrement land tool size
                gLandToolSize = std::max(MINIMUM_TOOL_SIZE, gLandToolSize - 1);

                // Invalidate the window
                Invalidate();
                break;
            case WIDX_INCREMENT:
                // Increment land tool size
                gLandToolSize = std::min(MAXIMUM_TOOL_SIZE, gLandToolSize + 1);

                // Invalidate the window
                Invalidate();
                break;
        }
    }

    void OnUpdate() override
    {
        // Close window if another tool is open
        if (!water_tool_is_active())
        {
            Close();
        }
    }

    void OnTextInput(rct_widgetindex widgetIndex, std::string_view text) override
    {
        int32_t size;
        char* end;

        if (widgetIndex != WIDX_PREVIEW)
        {
            return;
        }

        size = strtol(std::string(text).c_str(), &end, 10);
        if (*end == '\0')
        {
            size = std::max(MINIMUM_TOOL_SIZE, size);
            size = std::min(MAXIMUM_TOOL_SIZE, size);
            gLandToolSize = size;

            Invalidate();
        }
    }

    void OnPrepareDraw() override
    {
        // Set the preview image button to be pressed down
        SetWidgetPressed(WIDX_PREVIEW, true);

        // Update the preview image
        widgets[WIDX_PREVIEW].image = LandTool::SizeToSpriteIndex(gLandToolSize);
    }

    void OnDraw(rct_drawpixelinfo& dpi) override
    {
        auto screenCoords = ScreenCoordsXY{ windowPos.x + window_water_widgets[WIDX_PREVIEW].midX(),
                                            windowPos.y + window_water_widgets[WIDX_PREVIEW].midY() };

        DrawWidgets(dpi);
        // Draw number for tool sizes bigger than 7
        if (gLandToolSize > MAX_TOOL_SIZE_WITH_SPRITE)
        {
            DrawTextBasic(
                &dpi, screenCoords - ScreenCoordsXY{ 0, 2 }, STR_LAND_TOOL_SIZE_VALUE, &gLandToolSize,
                { TextAlignment::CENTRE });
        }

        if (!(gParkFlags & PARK_FLAGS_NO_MONEY))
        {
            // Draw raise cost amount
            screenCoords = { window_water_widgets[WIDX_PREVIEW].midX() + windowPos.x,
                             window_water_widgets[WIDX_PREVIEW].bottom + windowPos.y + 5 };
            if (gWaterToolRaiseCost != MONEY32_UNDEFINED && gWaterToolRaiseCost != 0)
            {
                DrawTextBasic(&dpi, screenCoords, STR_RAISE_COST_AMOUNT, &gWaterToolRaiseCost, { TextAlignment::CENTRE });
            }
            screenCoords.y += 10;

            // Draw lower cost amount
            if (gWaterToolLowerCost != MONEY32_UNDEFINED && gWaterToolLowerCost != 0)
            {
                DrawTextBasic(&dpi, screenCoords, STR_LOWER_COST_AMOUNT, &gWaterToolLowerCost, { TextAlignment::CENTRE });
            }
        }
    }

private:
    void InputSize()
    {
        TextInputDescriptionArgs[0] = MINIMUM_TOOL_SIZE;
        TextInputDescriptionArgs[1] = MAXIMUM_TOOL_SIZE;
        window_text_input_open(this, WIDX_PREVIEW, STR_SELECTION_SIZE, STR_ENTER_SELECTION_SIZE, STR_NONE, STR_NONE, 3);
    }
};

rct_window* window_water_open()
{
    return WindowFocusOrCreate<WaterWindow>(WC_WATER, ScreenCoordsXY(context_get_width() - WW, 29), WW, WH, 0);
}
