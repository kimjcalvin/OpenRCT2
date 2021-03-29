/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#ifdef ENABLE_SCRIPTING

#    include <openrct2/drawing/Drawing.h>
#    include <openrct2/scripting/Duktape.hpp>

namespace OpenRCT2::Scripting
{
    class ScGraphicsContext
    {
    private:
        duk_context* _ctx{};
        rct_drawpixelinfo _dpi{};

        std::optional<colour_t> _colour{};
        std::optional<colour_t> _secondaryColour{};
        std::optional<colour_t> _ternaryColour{};
        std::optional<uint8_t> _paletteId{};
        uint8_t _stroke{};
        uint8_t _fill{};

    public:
        ScGraphicsContext(duk_context* ctx, const rct_drawpixelinfo& dpi)
            : _ctx(ctx)
            , _dpi(dpi)
        {
        }

        static void Register(duk_context* ctx)
        {
            dukglue_register_property(ctx, &ScGraphicsContext::colour_get, &ScGraphicsContext::colour_set, "colour");
            dukglue_register_property(
                ctx, &ScGraphicsContext::secondaryColour_get, &ScGraphicsContext::secondaryColour_set, "secondaryColour");
            dukglue_register_property(
                ctx, &ScGraphicsContext::ternaryColour_get, &ScGraphicsContext::ternaryColour_set, "ternaryColour");
            dukglue_register_property(ctx, &ScGraphicsContext::paletteId_get, &ScGraphicsContext::paletteId_set, "paletteId");
            dukglue_register_property(ctx, &ScGraphicsContext::fill_get, &ScGraphicsContext::fill_set, "fill");
            dukglue_register_property(ctx, &ScGraphicsContext::stroke_get, &ScGraphicsContext::stroke_set, "stroke");
            dukglue_register_property(ctx, &ScGraphicsContext::width_get, nullptr, "width");
            dukglue_register_property(ctx, &ScGraphicsContext::height_get, nullptr, "height");

            dukglue_register_method(ctx, &ScGraphicsContext::getImage, "getImage");
            dukglue_register_method(ctx, &ScGraphicsContext::measureText, "measureText");

            dukglue_register_method(ctx, &ScGraphicsContext::box, "box");
            dukglue_register_method(ctx, &ScGraphicsContext::clear, "clear");
            dukglue_register_method(ctx, &ScGraphicsContext::clip, "clip");
            dukglue_register_method(ctx, &ScGraphicsContext::image, "image");
            dukglue_register_method(ctx, &ScGraphicsContext::line, "line");
            dukglue_register_method(ctx, &ScGraphicsContext::rect, "rect");
            dukglue_register_method(ctx, &ScGraphicsContext::text, "text");
            dukglue_register_method(ctx, &ScGraphicsContext::well, "well");
        }

    private:
        DukValue colour_get() const
        {
            return ToDuk(_ctx, _colour);
        }

        void colour_set(DukValue value)
        {
            if (value.type() == DukValue::NUMBER)
                _colour = static_cast<colour_t>(value.as_int());
            else
                _colour = {};
        }

        DukValue secondaryColour_get() const
        {
            return ToDuk(_ctx, _secondaryColour);
        }

        void secondaryColour_set(DukValue value)
        {
            if (value.type() == DukValue::NUMBER)
                _secondaryColour = static_cast<colour_t>(value.as_int());
            else
                _secondaryColour = {};
        }

        DukValue ternaryColour_get() const
        {
            return ToDuk(_ctx, _ternaryColour);
        }

        void ternaryColour_set(DukValue value)
        {
            if (value.type() == DukValue::NUMBER)
                _ternaryColour = static_cast<colour_t>(value.as_int());
            else
                _ternaryColour = {};
        }

        DukValue paletteId_get() const
        {
            return ToDuk(_ctx, _paletteId);
        }

        void paletteId_set(DukValue value)
        {
            if (value.type() == DukValue::NUMBER)
                _paletteId = static_cast<uint8_t>(value.as_int());
            else
                _paletteId = {};
        }

        uint8_t fill_get() const
        {
            return _fill;
        }

        void fill_set(uint8_t value)
        {
            _fill = value;
        }

        uint8_t stroke_get() const
        {
            return _stroke;
        }

        void stroke_set(uint8_t value)
        {
            _stroke = value;
        }

        int32_t width_get() const
        {
            return _dpi.width;
        }

        int32_t height_get() const
        {
            return _dpi.height;
        }

        DukValue getImage(uint32_t id)
        {
            auto* g1 = gfx_get_g1_element(id);
            if (g1 == nullptr)
            {
                return ToDuk(_ctx, undefined);
            }
            else
            {
                DukObject obj(_ctx);
                obj.Set("id", id);
                obj.Set("offset", ToDuk<ScreenCoordsXY>(_ctx, { g1->x_offset, g1->y_offset }));
                obj.Set("width", g1->width);
                obj.Set("height", g1->height);

                obj.Set("isBMP", (g1->flags & G1_FLAG_BMP) != 0);
                obj.Set("isRLE", (g1->flags & G1_FLAG_RLE_COMPRESSION) != 0);
                obj.Set("isPalette", (g1->flags & G1_FLAG_PALETTE) != 0);
                obj.Set("noZoom", (g1->flags & G1_FLAG_NO_ZOOM_DRAW) != 0);

                if (g1->flags & G1_FLAG_HAS_ZOOM_SPRITE)
                {
                    obj.Set("nextZoomId", id - g1->zoomed_offset);
                }
                else
                {
                    obj.Set("nextZoomId", undefined);
                }
                return obj.Take();
            }
        }

        DukValue measureText(const std::string& text)
        {
            auto width = gfx_get_string_width(text, FontSpriteBase::MEDIUM);
            auto height = string_get_height_raw(text.c_str(), FontSpriteBase::MEDIUM);
            return ToDuk<ScreenSize>(_ctx, { width, height });
        }

        void box(int32_t x, int32_t y, int32_t width, int32_t height)
        {
            gfx_fill_rect_inset(&_dpi, { x, y, x + width - 1, y + height - 1 }, _colour.value_or(0), 0);
        }

        void well(int32_t x, int32_t y, int32_t width, int32_t height)
        {
            gfx_fill_rect_inset(
                &_dpi, { x, y, x + width - 1, y + height - 1 }, _colour.value_or(0),
                INSET_RECT_FLAG_BORDER_INSET | INSET_RECT_FLAG_FILL_DONT_LIGHTEN);
        }

        void clear()
        {
            gfx_clear(&_dpi, _fill);
        }

        void clip(int32_t x, int32_t y, int32_t width, int32_t height)
        {
            rct_drawpixelinfo newDpi;
            clip_drawpixelinfo(&newDpi, &_dpi, { x, y }, width, height);
            _dpi = newDpi;
        }

        void image(uint32_t id, int32_t x, int32_t y)
        {
            ImageId img;
            img = img.WithIndex(id);
            if (_paletteId)
            {
                img = img.WithRemap(*_paletteId);
            }
            else
            {
                if (_colour)
                {
                    img = img.WithPrimary(*_colour);
                }
                if (_secondaryColour)
                {
                    img = img.WithSecondary(*_secondaryColour);
                }
            }

            gfx_draw_sprite(&_dpi, img.WithTertiary(_ternaryColour.value_or(0)), { x, y });
        }

        void line(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
        {
            gfx_draw_line(&_dpi, { { x1, y1 }, { x2, y2 } }, _stroke);
        }

        void rect(int32_t x, int32_t y, int32_t width, int32_t height)
        {
            if (_stroke != 0)
            {
                line(x, y, x + width, y);
                line(x + width - 1, y + 1, x + width - 1, y + height - 1);
                line(x, y + height - 1, x + width, y + height - 1);
                line(x, y + 1, x, y + height - 1);

                x++;
                y++;
                width -= 2;
                height -= 2;
            }
            if (_fill != 0)
            {
                gfx_fill_rect(&_dpi, { x, y, x + width - 1, y + height - 1 }, _fill);
            }
        }

        void text(const std::string& text, int32_t x, int32_t y)
        {
            gfx_draw_string(&_dpi, { x, y }, text.c_str(), { _colour.value_or(0) });
        }
    };
} // namespace OpenRCT2::Scripting

#endif
