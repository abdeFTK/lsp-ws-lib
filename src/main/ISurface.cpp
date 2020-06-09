/*
 * ISurface.cpp
 *
 *  Created on: 25 окт. 2016 г.
 *      Author: sadko
 */

#include <lsp-plug.in/ws/ISurface.h>
#include <stdlib.h>

namespace lsp
{
    namespace ws
    {
        ISurface::ISurface(size_t width, size_t height, surface_type_t type)
        {
            nWidth      = width;
            nHeight     = height;
            nType       = type;
            nStride     = nWidth * sizeof(uint32_t);
            pData       = NULL;
        }

        ISurface::ISurface()
        {
            nWidth      = 0;
            nHeight     = 0;
            nType       = ST_UNKNOWN;
            nStride     = nWidth * sizeof(uint32_t);
            pData       = NULL;
        }

        ISurface::~ISurface()
        {
            nWidth      = 0;
            nHeight     = 0;
            nType       = ST_UNKNOWN;
            nStride     = nWidth * sizeof(uint32_t);
            pData       = NULL;
        }

        ISurface *ISurface::create(size_t width, size_t height)
        {
            return new ISurface(width, height, ST_UNKNOWN);
        }

        ISurface *ISurface::create_copy()
        {
            return new ISurface(nWidth, nHeight, ST_UNKNOWN);
        }

        void ISurface::destroy()
        {
        }

        IGradient *ISurface::linear_gradient(float x0, float y0, float x1, float y1)
        {
            return new IGradient();
        }

        IGradient *ISurface::radial_gradient(float cx0, float cy0, float r0, float cx1, float cy1, float r1)
        {
            return new IGradient();
        }

        void ISurface::draw(ISurface *s, float x, float y)
        {
        }

        void ISurface::draw(ISurface *s, float x, float y, float sx, float sy)
        {
        }

        void ISurface::draw_alpha(ISurface *s, float x, float y, float sx, float sy, float a)
        {
        }

        void ISurface::draw_rotate_alpha(ISurface *s, float x, float y, float sx, float sy, float ra, float a)
        {

        }

        void ISurface::draw_clipped(ISurface *s, float x, float y, float sx, float sy, float sw, float sh)
        {
        }

        void ISurface::fill_rect(const Color &color, float left, float top, float width, float height)
        {
        }

        void ISurface::fill_rect(const Color &color, const ws::rectangle_t *r)
        {
            fill_rect(color, r->nLeft, r->nTop, r->nWidth, r->nHeight);
        }

        void ISurface::fill_rect(IGradient *g, float left, float top, float width, float height)
        {
        }

        void ISurface::fill_rect(IGradient *g, const ws::rectangle_t *r)
        {
            fill_rect(g, r->nLeft, r->nTop, r->nWidth, r->nHeight);
        }

        void ISurface::wire_rect(const Color &color, float left, float top, float width, float height, float line_width)
        {
        }

        void ISurface::wire_rect(IGradient *g, float left, float top, float width, float height, float line_width)
        {
        }

        void ISurface::wire_round_rect(float left, float top, float width, float height, float radius, size_t mask, float line_width, const Color &color)
        {
        }

        void ISurface::wire_round_rect(float left, float top, float width, float height, float radius, size_t mask, float line_width, IGradient *g)
        {
        }

        void ISurface::fill_round_rect(const Color &color, size_t mask, float left, float top, float width, float height, float radius)
        {
        }

        void ISurface::fill_round_rect(IGradient *g, size_t mask, float left, float top, float width, float height, float radius)
        {
        }

        void ISurface::full_rect(float left, float top, float width, float height, float line_width, const Color &color)
        {
        }

        void ISurface::fill_sector(float cx, float cy, float radius, float angle1, float angle2, const Color &color)
        {
        }

        void ISurface::fill_triangle(float x0, float y0, float x1, float y1, float x2, float y2, IGradient *g)
        {
        }

        void ISurface::fill_triangle(float x0, float y0, float x1, float y1, float x2, float y2, const Color &color)
        {
        }

        bool ISurface::get_font_parameters(const Font &f, font_parameters_t *fp)
        {
            return false;
        }

        bool ISurface::get_text_parameters(const Font &f, text_parameters_t *tp, const char *text)
        {
            return false;
        }

        bool ISurface::get_text_parameters(const Font &f, text_parameters_t *tp, const LSPString *text)
        {
            if (text == NULL)
                return false;
            return get_text_parameters(f, tp, text, 0, text->length());
        }

        bool ISurface::get_text_parameters(const Font &f, text_parameters_t *tp, const LSPString *text, ssize_t first)
        {
            if (text == NULL)
                return false;
            return get_text_parameters(f, tp, text, first, text->length());
        }

        bool ISurface::get_text_parameters(const Font &f, text_parameters_t *tp, const LSPString *text, ssize_t first, ssize_t last)
        {
            return false;
        }

        void ISurface::clear(const Color &color)
        {
        }

        void ISurface::begin()
        {
        }

        void ISurface::end()
        {
        }

        void ISurface::clear_rgb(uint32_t color)
        {
        }

        void ISurface::clear_rgba(uint32_t color)
        {
        }

        void ISurface::out_text(const Font &f, const Color &color, float x, float y, const char *text)
        {
        }

        void ISurface::out_text(const Font &f, const Color &color, float x, float y, const LSPString *text)
        {
            if (text == NULL)
                return;
            out_text(f, color, x, y, text, 0, text->length());
        }

        void ISurface::out_text(const Font &f, const Color &color, float x, float y, const LSPString *text, ssize_t first)
        {
            if (text == NULL)
                return;
            out_text(f, color, x, y, text, first, text->length());
        }

        void ISurface::out_text(const Font &f, const Color &color, float x, float y, const LSPString *text, ssize_t first, ssize_t last)
        {
        }

        void ISurface::out_text_relative(const Font &f, const Color &color, float x, float y, float dx, float dy, const char *text)
        {
        }

        void ISurface::out_text_relative(const Font &f, const Color &color, float x, float y, float dx, float dy, const LSPString *text)
        {
            if (text == NULL)
                return;
            out_text_relative(f, color, x, y, dx, dy, text, 0, text->length());
        }

        void ISurface::out_text_relative(const Font &f, const Color &color, float x, float y, float dx, float dy, const LSPString *text, ssize_t first)
        {
            if (text == NULL)
                return;
            out_text_relative(f, color, x, y, dx, dy, text, first, text->length());
        }

        void ISurface::out_text_relative(const Font &f, const Color &color, float x, float y, float dx, float dy, const LSPString *text, ssize_t first, ssize_t last)
        {
        }

        void ISurface::square_dot(float x, float y, float width, const Color &color)
        {
        }

        void ISurface::square_dot(float x, float y, float width, float r, float g, float b, float a)
        {
        }

        void ISurface::line(float x0, float y0, float x1, float y1, float width, const Color &color)
        {
        }

        void ISurface::line(float x0, float y0, float x1, float y1, float width, IGradient *g)
        {
        }

        void ISurface::parametric_line(float a, float b, float c, float width, const Color &color)
        {
        }

        void ISurface::parametric_line(float a, float b, float c, float left, float right, float top, float bottom, float width, const Color &color)
        {
        }

        void ISurface::parametric_bar(float a1, float b1, float c1, float a2, float b2, float c2,
                float left, float right, float top, float bottom, IGradient *gr)
        {
        }

        void ISurface::wire_arc(float x, float y, float r, float a1, float a2, float width, const Color &color)
        {
        }

        void ISurface::fill_frame(const Color &color,
                        float fx, float fy, float fw, float fh,
                        float ix, float iy, float iw, float ih
                    )
        {
        }

        void ISurface::fill_frame(const Color &color, const ws::rectangle_t *out, const ws::rectangle_t *in)
        {
            fill_frame(color,
                    out->nLeft, out->nTop, out->nWidth, out->nHeight,
                    in->nLeft, in->nTop, in->nWidth, in->nHeight
            );
        }

        void ISurface::fill_round_frame(
                const Color &color,
                float radius, size_t flags,
                float fx, float fy, float fw, float fh,
                float ix, float iy, float iw, float ih
            )
        {
        }

        void ISurface::fill_round_frame(
                const Color &color, float radius, size_t flags,
                const ws::rectangle_t *out, const ws::rectangle_t *in
            )
        {
            fill_round_frame(color, radius, flags,
                    out->nLeft, out->nTop, out->nWidth, out->nHeight,
                    in->nLeft, in->nTop, in->nWidth, in->nHeight
            );
        }

        void ISurface::fill_poly(const float *x, const float *y, size_t n, const Color & color)
        {
        }

        void ISurface::fill_poly(const float *x, const float *y, size_t n, IGradient *gr)
        {
        }

        void ISurface::wire_poly(const float *x, const float *y, size_t n, float width, const Color & color)
        {
        }

        void ISurface::draw_poly(const float *x, const float *y, size_t n, float width, const Color &fill, const Color &wire)
        {
        }

        void ISurface::fill_circle(float x, float y, float r, const Color & color)
        {
        }

        void ISurface::fill_circle(float x, float y, float r, IGradient *g)
        {
        }

        void ISurface::clip_begin(float x, float y, float w, float h)
        {
        }

        void ISurface::clip_begin(const ws::rectangle_t *area)
        {
            clip_begin(area->nLeft, area->nTop, area->nWidth, area->nHeight);
        }

        void ISurface::clip_end()
        {
        }
    
        bool ISurface::get_antialiasing()
        {
            return false;
        }
    
        bool ISurface::set_antialiasing(bool set)
        {
            return false;
        }

        surf_line_cap_t ISurface::get_line_cap()
        {
            return SURFLCAP_BUTT;
        }

        surf_line_cap_t ISurface::set_line_cap(surf_line_cap_t lc)
        {
            return SURFLCAP_BUTT;
        }

        size_t ISurface::stride()
        {
            return nStride;
        }

        void *ISurface::data()
        {
            return pData;
        }

        void *ISurface::row(size_t row)
        {
            if ((row >= nHeight) || (pData == NULL))
                return NULL;
            return &pData[row * nStride];
        }

        void *ISurface::start_direct()
        {
            return NULL;
        }

        void ISurface::end_direct()
        {
        }
    }

} /* namespace lsp */
