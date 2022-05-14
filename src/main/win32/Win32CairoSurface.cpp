#include <lsp-plug.in/common/types.h>
#include <lsp-plug.in/common/debug.h>

#include <lsp-plug.in/stdlib/math.h>

#include <windows.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <cairo/cairo-ft.h>

#include <private/win32/Win32CairoGradient.h>
#include <private/win32/Win32CairoSurface.h>

#define PELS_72DPI  ((LONG)(72. / 0.0254))


namespace lsp
{
    namespace ws
    {
        namespace win32
        {
            static inline cairo_antialias_t decode_antialiasing(const Font &f)
            {
                switch (f.antialiasing())
                {
                    case FA_DISABLED: return CAIRO_ANTIALIAS_NONE;
                    case FA_ENABLED: return CAIRO_ANTIALIAS_GOOD;
                    default: break;
                }
                return CAIRO_ANTIALIAS_DEFAULT;
            }

            Win32CairoSurface::Win32CairoSurface(Win32Display *dpy, HWND wnd, size_t width, size_t height):
                ISurface(width, height, ST_UNKNOWN)
            {
                pDisplay        = dpy;
                pCR             = NULL;
                pFO             = NULL;
                pHdc            = NULL;
                pWnd            = wnd;
                rendering = false;
                pSurface        = ::cairo_win32_surface_create_with_dib(CAIRO_FORMAT_ARGB32, width, height);
                //resize(0,0);
            }

            Win32CairoSurface::Win32CairoSurface(Win32Display *dpy, size_t width, size_t height):
                ISurface(width, height, ST_IMAGE)
            {
                pDisplay        = dpy;
                pCR             = NULL;
                pFO             = NULL;
                pHdc            = NULL;
                pWnd            = NULL;
                rendering = false;
                pSurface        = ::cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
                nStride         = cairo_image_surface_get_stride(pSurface);
            }

            ISurface *Win32CairoSurface::create(size_t width, size_t height)
            {
                //lsp_debug("CREATE SURFACE CALLED");
                return new Win32CairoSurface(pDisplay, width, height);
            }

            ISurface *Win32CairoSurface::create_copy()
            {
                Win32CairoSurface *s = new Win32CairoSurface(pDisplay, nWidth, nHeight);
                if (s == NULL)
                    return NULL;

                //lsp_debug("DEBUG");
                // Draw one surface on another
                s->begin();
                    ::cairo_set_source_surface(s->pCR, pSurface, 0.0f, 0.0f);
                    ::cairo_paint(s->pCR);
                s->end();

                return s;
            }

            IGradient *Win32CairoSurface::linear_gradient(float x0, float y0, float x1, float y1)
            {
                //lsp_debug("DEBUG");
                return new Win32CairoLinearGradient(x0, y0, x1, y1);
            }

            IGradient *Win32CairoSurface::radial_gradient(float cx0, float cy0, float r0, float cx1, float cy1, float r1)
            {
                //lsp_debug("DEBUG");
                return new Win32CairoRadialGradient(cx0, cy0, r0, cx1, cy1, r1);
            }

            Win32CairoSurface::~Win32CairoSurface()
            {
                //lsp_debug("DEBUG");
                destroy_context();
            }

            void Win32CairoSurface::destroy_context()
            {
                bitmapData = NULL;
                if (pFO != NULL)
                {
                    cairo_font_options_destroy(pFO);
                    pFO             = NULL;
                }
                if (pCR != NULL)
                {
                    cairo_destroy(pCR);
                    pCR             = NULL;
                }
                if (pSurface != NULL)
                {
                    cairo_surface_destroy(pSurface);
                    pSurface        = NULL;
                }
            }

            void Win32CairoSurface::destroy()
            {
                destroy_context();
            }

            bool Win32CairoSurface::sizeSynced() {
                if (pSurface != NULL) {
                    cairo_surface_t* offSurface = cairo_win32_surface_get_image(pSurface);
                    if (offSurface == NULL) {
                        return false;
                    }
                    int offwidth = cairo_image_surface_get_width(offSurface);
                    int offheight = cairo_image_surface_get_height(offSurface);

                    //lsp_debug("Sync surface Size : oldWidth %ld, oldHeight %ld, width %ld, height %ld", offwidth, offheight, nWidth, nHeight);

                    return (offwidth == nWidth && offheight == nHeight);
                }
                return false;
            }

            bool Win32CairoSurface::syncSize() {
                if (nType != ST_IMAGE && pSurface != NULL)
                {
                    // cairo_surface_t* offSurface = cairo_win32_surface_get_image(pSurface);
                    // if (offSurface == NULL) {
                    //     return false;
                    // }
                    // int offwidth = cairo_image_surface_get_width(offSurface);
                    // int offheight = cairo_image_surface_get_height(offSurface);

                    //lsp_debug("Sync surface Size : oldWidth %ld, oldHeight %ld, width %ld, height %ld", offwidth, offheight, nWidth, nHeight);

                    if (!sizeSynced()) {
                        cairo_surface_t* oldSurf = pSurface;
                        cairo_t* oldCr = pCR;
                        pCR = NULL;
                        pSurface        = ::cairo_win32_surface_create_with_dib(CAIRO_FORMAT_ARGB32, nWidth, nHeight);

                        if (oldCr != NULL)
                        {
                            cairo_destroy(oldCr);
                            oldCr             = NULL;
                        }
                        if (oldSurf != NULL)
                        {
                            cairo_surface_destroy(oldSurf);
                            oldSurf = NULL;
                        }
                        return true;
                    }
                }
                return false;
            }

            bool Win32CairoSurface::resize(size_t width, size_t height)
            {
                nWidth = width;
                nHeight = height;
                if (nType == ST_IMAGE)
                {
                    int offwidth = cairo_image_surface_get_width(pSurface);
                    int offheight = cairo_image_surface_get_height(pSurface);

                    //lsp_debug("Sync surface Size : oldWidth %ld, oldHeight %ld, width %ld, height %ld", offwidth, offheight, nWidth, nHeight);

                    if (offwidth != nWidth || offheight != nHeight) {

                        // Create new surface and cairo
                        cairo_surface_t *s  = ::cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
                        if (s == NULL)
                            return false;
                        cairo_t *cr         = ::cairo_create(s);
                        if (cr == NULL)
                        {
                            cairo_surface_destroy(s);
                            return false;
                        }

                        // Draw previous content
                        ::cairo_set_source_surface(cr, pSurface, 0, 0);
                        ::cairo_fill(cr);

                        // Destroy previously used context
                        destroy_context();

                        // Update context
                        pSurface            = s;
                        if (pCR != NULL)
                        {
                            ::cairo_destroy(pCR);
                            pCR                 = cr;
                        }
                        else
                            ::cairo_destroy(cr);

                    }
                } else {
                    // if (pSurface != NULL) {
                    //     // HDC sDC = cairo_win32_surface_get_dc(pSurface);
                    //     // ValidateRect(static_cast<Win32Display *>(pDisplay)->hRootWnd,NULL);
                    //     // ValidateRgn(static_cast<Win32Display *>(pDisplay)->hRootWnd,NULL);
                    //     // RECT rc;
                    //     // GetWindowRect(static_cast<Win32Display *>(pDisplay)->hRootWnd, &rc); 
                    //     // SetMapMode(sDC, MM_ANISOTROPIC); 
                    //     // SetWindowExtEx(sDC,  rc.right, rc.bottom, NULL); 
                    //     // SetViewportExtEx(sDC, rc.right, rc.bottom, NULL); 
                    //     ::cairo_surface_flush(pSurface);
                    //     cairo_surface_mark_dirty(pSurface);
                    // }





                    // cairo_surface_t* offSurface = cairo_win32_surface_get_image(pSurface);
                    // int offwidth = cairo_image_surface_get_width(offSurface);
                    // int offheight = cairo_image_surface_get_width(offSurface);

                    // if (offwidth != width || offheight != height) {
                    //     cairo_surface_t* oldSurf = pSurface;
                    //     cairo_t* oldCr = pCR;
                    //     pCR = NULL;
                    //     pSurface        = ::cairo_win32_surface_create_with_dib(CAIRO_FORMAT_ARGB32, width, height);

                    //     if (oldCr != NULL)
                    //     {
                    //         cairo_destroy(oldCr);
                    //         oldCr             = NULL;
                    //     }
                    //     if (oldSurf != NULL)
                    //     {
                    //         cairo_surface_destroy(oldSurf);
                    //         oldSurf = NULL;
                    //     }
                    // }
                    
                }
                return true;
            }

            void Win32CairoSurface::draw(ISurface *s, float x, float y)
            {
                //lsp_debug("DEBUG");
                surface_type_t type = s->type();
                // if ((type != ST_XLIB) && (type != ST_IMAGE))
                //     return;
                if (pCR == NULL)
                    return;
                Win32CairoSurface *cs = static_cast<Win32CairoSurface *>(s);
                if (cs->pSurface == NULL)
                    return;

                //lsp_debug("DEBUG");

                // Draw one surface on another
                ::cairo_set_source_surface(pCR, cs->pSurface, x, y);
                ::cairo_paint(pCR);
            }

            void Win32CairoSurface::draw(ISurface *s, float x, float y, float sx, float sy)
            {
                //lsp_debug("DEBUG");
                surface_type_t type = s->type();
                // if ((type != ST_XLIB) && (type != ST_IMAGE))
                //     return;
                if (pCR == NULL)
                    return;
                Win32CairoSurface *cs = static_cast<Win32CairoSurface *>(s);
                if (cs->pSurface == NULL)
                    return;


                //lsp_debug("DEBUG");

                // Draw one surface on another
                ::cairo_save(pCR);
                if (sx < 0.0f)
                    x       -= sx * s->width();
                if (sy < 0.0f)
                    y       -= sy * s->height();
                ::cairo_translate(pCR, x, y);
                ::cairo_scale(pCR, sx, sy);
                ::cairo_set_source_surface(pCR, cs->pSurface, 0.0f, 0.0f);
                ::cairo_paint(pCR);
                ::cairo_restore(pCR);
            }

            void Win32CairoSurface::draw_alpha(ISurface *s, float x, float y, float sx, float sy, float a)
            {
                surface_type_t type = s->type();
                // if ((type != ST_XLIB) && (type != ST_IMAGE))
                //     return;
                if (pCR == NULL)
                    return;
                Win32CairoSurface *cs = static_cast<Win32CairoSurface *>(s);
                if (cs->pSurface == NULL)
                    return;

                //lsp_debug("DEBUG");

                // Draw one surface on another
                ::cairo_save(pCR);
                if (sx < 0.0f)
                    x       -= sx * s->width();
                if (sy < 0.0f)
                    y       -= sy * s->height();
                ::cairo_translate(pCR, x, y);
                ::cairo_scale(pCR, sx, sy);
                ::cairo_set_source_surface(pCR, cs->pSurface, 0.0f, 0.0f);
                ::cairo_paint_with_alpha(pCR, 1.0f - a);
                ::cairo_restore(pCR);
            }

            void Win32CairoSurface::draw_rotate_alpha(ISurface *s, float x, float y, float sx, float sy, float ra, float a)
            {
                surface_type_t type = s->type();
                // if ((type != ST_XLIB) && (type != ST_IMAGE))
                //     return;
                if (pCR == NULL)
                    return;
                Win32CairoSurface *cs = static_cast<Win32CairoSurface *>(s);
                if (cs->pSurface == NULL)
                    return;

                //lsp_debug("DEBUG");

                // Draw one surface on another
                ::cairo_save(pCR);
                ::cairo_translate(pCR, x, y);
                ::cairo_scale(pCR, sx, sy);
                ::cairo_rotate(pCR, ra);
                ::cairo_set_source_surface(pCR, cs->pSurface, 0.0f, 0.0f);
                ::cairo_paint_with_alpha(pCR, 1.0f - a);
                ::cairo_restore(pCR);
            }

            void Win32CairoSurface::draw_clipped(ISurface *s, float x, float y, float sx, float sy, float sw, float sh)
            {
                surface_type_t type = s->type();
                // if ((type != ST_XLIB) && (type != ST_IMAGE))
                //     return;
                if (pCR == NULL)
                    return;
                Win32CairoSurface *cs = static_cast<Win32CairoSurface *>(s);
                if (cs->pSurface == NULL)
                    return;

                //lsp_debug("DEBUG");

                // Draw one surface on another
                ::cairo_save(pCR);
                ::cairo_set_source_surface(pCR, cs->pSurface, x - sx, y - sy);
                ::cairo_rectangle(pCR, x, y, sw, sh);
                ::cairo_fill(pCR);
                ::cairo_restore(pCR);
            }

            void Win32CairoSurface::begin()
            {
                //lsp_debug("begin");
                // Force end() call
                //end();
                syncSize();
                //lsp_debug("synced");
                endWithoutUpdate();
                //lsp_debug("endWithoutUpdate");

                if (!rendering) {
                // Create cairo objects
                    pCR             = ::cairo_create(pSurface);
                } else {
                    lsp_debug("ALREADY RENDERING");
                }
                if (pCR == NULL)
                    return;
                pFO             = ::cairo_font_options_create();
                if (pFO == NULL)
                    return;

                ////lsp_debug("DEBUG");

                // Initialize settings
                ::cairo_set_antialias(pCR, CAIRO_ANTIALIAS_DEFAULT);
                ::cairo_set_line_join(pCR, CAIRO_LINE_JOIN_BEVEL);
            }

            void Win32CairoSurface::endWithoutUpdate() {
                if (pCR == NULL)
                    return;

                if (pFO != NULL)
                {
                    cairo_font_options_destroy(pFO);
                    pFO             = NULL;
                }
                if (pCR != NULL)
                {
                    cairo_destroy(pCR);
                    pCR             = NULL;
                }

                // ////lsp_debug("DEBUG");
                // if (!rendering && nType != ST_IMAGE) {
                //     cairo_surface_t* offSurface = cairo_win32_surface_get_image(pSurface);
                //     if (offSurface == NULL) {
                //         return;
                //     }
                //     bitmapData = reinterpret_cast<uint8_t *>(cairo_image_surface_get_data(offSurface));
                //     ::cairo_surface_flush(pSurface);
                //     //cairo_surface_mark_dirty(pSurface);
                // } else if (nType != ST_IMAGE) {
                //     lsp_debug("ALREADY RENDERING");
                // }
                // if (nType == ST_IMAGE) {
                if (pSurface != NULL) {
                    ::cairo_surface_flush(pSurface);
                }
                    
                //}
            }

            void Win32CairoSurface::renderOffscreen(HDC dc, PAINTSTRUCT& paintStruct, int width, int height) {
                if (pSurface != NULL && bitmapData != NULL && sizeSynced()) {

                    int x = paintStruct.rcPaint.left;
                    int y = paintStruct.rcPaint.top;
                    int w = paintStruct.rcPaint.right - x;
                    int h = paintStruct.rcPaint.bottom - y;
                    
                    // int imageWidth = cairo_image_surface_get_width(pSurface);
                    // int imageHeight = cairo_image_surface_get_height(pSurface);
                    // cairo_surface_t* offSurface = cairo_win32_surface_get_image(pSurface);
                    // uint8_t * bitmapData = reinterpret_cast<uint8_t *>(cairo_image_surface_get_data(offSurface));

                    //SetMapMode (dc, MM_TEXT);

                    memset(&bitmap_info, 0, sizeof(bitmap_info));
                    bitmap_info.bV4Size     = sizeof (BITMAPV4HEADER);
                    bitmap_info.bV4Width = width == 0 ? 1 : width;
                    bitmap_info.bV4Height = height == 0 ? -1 : - height; /* top-down */
                    bitmap_info.bV4CSType = 1;
                    bitmap_info.bV4BitCount = 32;
                    bitmap_info.bV4Planes = 1;
                    bitmap_info.bV4AlphaMask      = 0xff000000;
                    bitmap_info.bV4RedMask        = 0xff0000;
                    bitmap_info.bV4GreenMask      = 0xff00;
                    bitmap_info.bV4BlueMask       = 0xff;
                    bitmap_info.bV4V4Compression  = BI_BITFIELDS;

                    // rendering = true;
                    // // uint8_t               bitmapCopy;
                    // memset(bitmapCopy, 0, sizeof(bitmapData));
                    // memcpy(bitmapCopy, bitmapData, sizeof(bitmapCopy));
                    // rendering = false;

                    // RGBQUAD bmiColors[2];

                    // bitmap_info.bmiColors = &bmiColors;
              
              
                    rendering = true;
                    int renderRes = StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height, bitmapData, (const BITMAPINFO*) &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
                    rendering = false;
                    
                    // if (renderRes == 0) {
                    //     lsp_debug("StretchDIBits failed. bitmap size : %ld", sizeof(bitmapData));
                    // } else {
                    //     lsp_debug("StretchDIBits SUCCESS");
                    // }
                }
            }

            void Win32CairoSurface::end()
            {
                ////lsp_debug("DEBUG");
                if (!rendering && nType != ST_IMAGE) {
                    //lsp_debug("cairo_win32_surface_get_image");
                    cairo_surface_t* offSurface = cairo_win32_surface_get_image(pSurface);
                    if (offSurface != NULL) {
                        bitmapData = reinterpret_cast<uint8_t *>(cairo_image_surface_get_data(offSurface));
                        //::cairo_surface_flush(pSurface);
                        //cairo_surface_mark_dirty(pSurface);
                        //lsp_debug("get bitmapData");
                    }
                } else if (nType != ST_IMAGE) {
                    lsp_debug("ALREADY RENDERING");
                }
                endWithoutUpdate();
               // if (!rendering) {
                if (nType != ST_IMAGE && !syncSize()) {
                    InvalidateRect(pWnd, NULL, false);
                }
                    //InvalidateRgn(pWnd, NULL, false);
                    //UpdateWindow(pWnd);
               // }
            }

            void Win32CairoSurface::clear_rgb(uint32_t rgb)
            {
                //lsp_debug("DEBUG");
                clear_rgba(rgb & 0xffffff);
            }

            void Win32CairoSurface::clear_rgba(uint32_t rgba)
            {
                if (pCR == NULL)
                    return;
                    
                //lsp_debug("DEBUG");

                cairo_operator_t op = cairo_get_operator(pCR);
                ::cairo_set_operator (pCR, CAIRO_OPERATOR_SOURCE);
                ::cairo_set_source_rgba(pCR,
                    float((rgba >> 16) & 0xff)/255.0f,
                    float((rgba >> 8) & 0xff)/255.0f,
                    float(rgba & 0xff)/255.0f,
                    float((rgba >> 24) & 0xff)/255.0f
                );
                ::cairo_paint(pCR);
                ::cairo_set_operator (pCR, op);
            }

            inline void Win32CairoSurface::setSourceRGB(const Color &col)
            {
                if (pCR == NULL)
                    return;
                
                //lsp_debug("DEBUG");
                ::cairo_set_source_rgb(pCR, col.red(), col.green(), col.blue());
            }

            inline void Win32CairoSurface::setSourceRGBA(const Color &col)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");
                ::cairo_set_source_rgba(pCR, col.red(), col.green(), col.blue(), 1.0f - col.alpha());
            }

            void Win32CairoSurface::clear(const Color &color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                cairo_operator_t op = ::cairo_get_operator(pCR);
                ::cairo_set_operator (pCR, CAIRO_OPERATOR_SOURCE);
                ::cairo_paint(pCR);
                ::cairo_set_operator (pCR, op);
            }

            void Win32CairoSurface::fill_rect(const Color &color, float left, float top, float width, float height)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                ::cairo_rectangle(pCR, left, top, width, height);
                ::cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_rect(IGradient *g, float left, float top, float width, float height)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(g);
                cg->apply(pCR);
                cairo_rectangle(pCR, left, top, width, height);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::wire_rect(const Color &color, float left, float top, float width, float height, float line_width)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                double w = cairo_get_line_width(pCR);
                cairo_set_line_width(pCR, line_width);
                cairo_rectangle(pCR, left + 0.5f, top + 0.5f, width, height);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, w);
            }

            void Win32CairoSurface::wire_rect(IGradient *g, float left, float top, float width, float height, float line_width)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(g);
                cg->apply(pCR);

                double w = cairo_get_line_width(pCR);
                cairo_set_line_width(pCR, line_width);
                cairo_rectangle(pCR, left + 0.5f, top + 0.5f, width, height);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, w);
            }

            void Win32CairoSurface::drawRoundRect(float xmin, float ymin, float width, float height, float radius, size_t mask)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                radius = lsp_max(0.0f, radius);
                const float xmax = xmin + width;
                const float ymax = ymin + height;

                if (mask & SURFMASK_LT_CORNER)
                {
                    cairo_move_to(pCR, xmin, ymin + radius);
                    cairo_arc(pCR, xmin + radius, ymin + radius, radius, M_PI, 1.5f*M_PI);
                }
                else
                    cairo_move_to(pCR, xmin, ymin);

                if (mask & SURFMASK_RT_CORNER)
                    cairo_arc(pCR, xmax - radius, ymin + radius, radius, 1.5f * M_PI, 2.0f * M_PI);
                else
                    cairo_line_to(pCR, xmax, ymin);

                if (mask & SURFMASK_RB_CORNER)
                    cairo_arc(pCR, xmax - radius, ymax - radius, radius, 0.0f, 0.5f * M_PI);
                else
                    cairo_line_to(pCR, xmax, ymax);

                if (mask & SURFMASK_LB_CORNER)
                    cairo_arc(pCR, xmin + radius, ymax - radius, radius, 0.5f * M_PI, M_PI);
                else
                    cairo_line_to(pCR, xmin, ymax);

                cairo_close_path(pCR);
            }

            void Win32CairoSurface::wire_round_rect(const Color &color, size_t mask, float radius, float left, float top, float width, float height, float line_width)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                double w = cairo_get_line_width(pCR);
                cairo_set_line_width(pCR, line_width);
                drawRoundRect(left, top, width, height, radius, mask);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, w);
            }

            void Win32CairoSurface::wire_round_rect(IGradient *g, size_t mask, float radius, float left, float top, float width, float height, float line_width)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(g);

                double w = cairo_get_line_width(pCR);
                cairo_set_line_width(pCR, line_width);
                cg->apply(pCR);
                drawRoundRect(left, top, width, height, radius, mask);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, w);
            }

            void Win32CairoSurface::wire_round_rect_inside(const Color &color, size_t mask, float radius, float left, float top, float width, float height, float line_width)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                double w = cairo_get_line_width(pCR);
                float lw2 = line_width * 0.5f;
                cairo_set_line_width(pCR, line_width);
                drawRoundRect(left + lw2, top + lw2, width - line_width, height - line_width, radius, mask);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, w);
            }

            void Win32CairoSurface::wire_round_rect_inside(IGradient *g, size_t mask, float radius, float left, float top, float width, float height, float line_width)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(g);

                double w = cairo_get_line_width(pCR);
                float lw2 = line_width * 0.5f;
                cairo_set_line_width(pCR, line_width);
                cg->apply(pCR);
                drawRoundRect(left + lw2, top + lw2, width - line_width, height - line_width, radius, mask);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, w);
            }

            void Win32CairoSurface::fill_round_rect(const Color &color, size_t mask, float radius, float left, float top, float width, float height)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                drawRoundRect(left, top, width, height, radius, mask);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_round_rect(const Color &color, size_t mask, float radius, const ws::rectangle_t *r)
            {
                if (pCR == NULL)
                    return;
                setSourceRGBA(color);
                drawRoundRect(r->nLeft, r->nTop, r->nWidth, r->nHeight, radius, mask);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_round_rect(IGradient *g, size_t mask, float radius, float left, float top, float width, float height)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(g);
                cg->apply(pCR);
                drawRoundRect(left, top, width, height, radius, mask);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_round_rect(IGradient *g, size_t mask, float radius, const ws::rectangle_t *r)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(g);
                cg->apply(pCR);
                drawRoundRect(r->nLeft, r->nTop, r->nWidth, r->nHeight, radius, mask);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::full_rect(float left, float top, float width, float height, float line_width, const Color &color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                cairo_set_line_width(pCR, line_width);
                cairo_rectangle(pCR, left, top, width, height);
                cairo_stroke_preserve(pCR);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_sector(float cx, float cy, float radius, float angle1, float angle2, const Color &color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                cairo_move_to(pCR, cx, cy);
                cairo_arc(pCR, cx, cy, radius, angle1, angle2);
                cairo_close_path(pCR);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_triangle(float x0, float y0, float x1, float y1, float x2, float y2, IGradient *g)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(g);
                cg->apply(pCR);
                cairo_move_to(pCR, x0, y0);
                cairo_line_to(pCR, x1, y1);
                cairo_line_to(pCR, x2, y2);
                cairo_close_path(pCR);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_triangle(float x0, float y0, float x1, float y1, float x2, float y2, const Color &color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                cairo_move_to(pCR, x0, y0);
                cairo_line_to(pCR, x1, y1);
                cairo_line_to(pCR, x2, y2);
                cairo_close_path(pCR);
                cairo_fill(pCR);
            }

            bool Win32CairoSurface::get_font_parameters(const Font &f, font_parameters_t *fp)
            {
                cairo_font_extents_t fe;
                fe.ascent           = 0.0;
                fe.descent          = 0.0;
                fe.height           = 0.0;
                fe.max_x_advance    = 0.0;
                fe.max_y_advance    = 0.0;

                if ((pCR != NULL) && (f.get_name() != NULL))
                {
                    // Set current font
                    font_context_t ctx;
                    set_current_font(&ctx, f);
                    {
                        // Get font parameters
                        cairo_font_extents(pCR, &fe);
                    }
                    unset_current_font(&ctx);
                }

                // Return result
                fp->Ascent          = fe.ascent;
                fp->Descent         = fe.descent;
                fp->Height          = fe.height;
                fp->MaxXAdvance     = fe.max_x_advance;
                fp->MaxYAdvance     = fe.max_y_advance;

                return true;
            }

            void Win32CairoSurface::set_current_font(font_context_t *ctx, const Font &f)
            {
                // Apply antialiasint to the font
                ctx->aa     = cairo_font_options_get_antialias(pFO);
                cairo_font_options_set_antialias(pFO, decode_antialiasing(f));
                cairo_set_font_options(pCR, pFO);

                // Try to select custom font face
                Win32Display::font_t *font = pDisplay->get_font(f.get_name());
                if (font != NULL)
                {
                    size_t index = (f.is_bold())    ? 0x1 : 0;
                    index       |= (f.is_italic())  ? 0x2 : 0;

                    cairo_font_face_t *ff = font->cr_face[index];
                    if (ff == NULL)
                    {
                        ff = cairo_ft_font_face_create_for_ft_face(font->ft_face, 0);
                        if (ff != NULL)
                        {
                            cairo_status_t cr_status = cairo_font_face_set_user_data (
                                ff, &pDisplay->sCairoUserDataKey,
                                font, (cairo_destroy_func_t) Win32Display::destroy_font_object
                            );

                            if (cr_status)
                            {
                                lsp_error("FT_MANAGE Error creating cairo font face for font '%s', error=%d", font->name, int(cr_status));
                                cairo_font_face_destroy(ff);
                                ff = NULL;
                            }
                        }

                        if (ff != NULL)
                        {
                            // Increment number of references (used by cairo)
                            font->cr_face[index]    = ff;
                            ++font->refs;

                            if (f.is_bold())
                                cairo_ft_font_face_set_synthesize(ff, CAIRO_FT_SYNTHESIZE_BOLD);
                            if (f.is_italic())
                                cairo_ft_font_face_set_synthesize(ff, CAIRO_FT_SYNTHESIZE_OBLIQUE);
                        }
                    }

                    if (ff != NULL)
                    {
                        cairo_set_font_face(pCR, ff);
                        cairo_set_font_size(pCR, f.get_size());

                        ctx->font   = font;
                        ctx->face   = ff;
                        return;
                    }
                }

                // Try to select fall-back font face
                cairo_select_font_face(pCR, f.get_name(),
                    (f.is_italic()) ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
                    (f.is_bold()) ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL
                );
                cairo_set_font_size(pCR, f.get_size());

                ctx->font   = NULL;
                ctx->face   = cairo_get_font_face(pCR);

                return;
            }

            void Win32CairoSurface::unset_current_font(font_context_t *ctx)
            {
                cairo_font_options_set_antialias(pFO, ctx->aa);
                cairo_set_font_face(pCR, NULL);

                ctx->face   = NULL;
                ctx->font   = NULL;
                ctx->aa     = CAIRO_ANTIALIAS_DEFAULT;
            }

            bool Win32CairoSurface::get_text_parameters(const Font &f, text_parameters_t *tp, const char *text)
            {
                // Initialize data structure
                cairo_text_extents_t te;
                te.x_bearing        = 0.0;
                te.y_bearing        = 0.0;
                te.width            = 0.0;
                te.height           = 0.0;
                te.x_advance        = 0.0;
                te.y_advance        = 0.0;

                if ((pCR != NULL) && (f.get_name() != NULL))
                {
                    // Set current font
                    font_context_t ctx;
                    set_current_font(&ctx, f);
                    {
                        // Get text parameters
                        cairo_glyph_t *glyphs = NULL;
                        int num_glyphs = 0;

                        cairo_scaled_font_t *scaled_font = cairo_get_scaled_font(pCR);
                        cairo_scaled_font_text_to_glyphs(scaled_font, 0.0, 0.0,
                                                   text, -1,
                                                   &glyphs, &num_glyphs,
                                                   NULL, NULL, NULL);
                        cairo_glyph_extents (pCR, glyphs, num_glyphs, &te);
                        cairo_glyph_free(glyphs);
                    }
                    unset_current_font(&ctx);
                }

                // Return result
                tp->XBearing        = te.x_bearing;
                tp->YBearing        = te.y_bearing;
                tp->Width           = te.width;
                tp->Height          = te.height;
                tp->XAdvance        = te.x_advance;
                tp->YAdvance        = te.y_advance;

                return true;
            }

            bool Win32CairoSurface::get_text_parameters(const Font &f, text_parameters_t *tp, const LSPString *text, ssize_t first, ssize_t last)
            {
                if (text == NULL)
                    return false;
                return get_text_parameters(f, tp, text->get_utf8(first, last));
            }

            void Win32CairoSurface::out_text(const Font &f, const Color &color, float x, float y, const char *text)
            {
                if ((pCR == NULL) || (f.get_name() == NULL) || (text == NULL))
                    return;

                // Set current font
                font_context_t ctx;
                set_current_font(&ctx, f);
                {
                    // Draw
                    cairo_move_to(pCR, x, y);
                    setSourceRGBA(color);
                    cairo_show_text(pCR, text);

                    if (f.is_underline())
                    {
                        cairo_text_extents_t te;
                        cairo_text_extents(pCR, text, &te);
                        float width = lsp_max(1.0f, f.get_size() / 12.0f);

                        cairo_set_line_width(pCR, width);

                        cairo_move_to(pCR, x, y + te.y_advance + 1 + width);
                        cairo_line_to(pCR, x + te.x_advance, y + te.y_advance + 1 + width);
                        cairo_stroke(pCR);
                    }
                }
                unset_current_font(&ctx);
            }

            void Win32CairoSurface::out_text(const Font &f, const Color &color, float x, float y, const LSPString *text, ssize_t first, ssize_t last)
            {
                if (text == NULL)
                    return;
                out_text(f, color, x, y, text->get_utf8(first, last));
            }

            void Win32CairoSurface::out_text_relative(const Font &f, const Color &color, float x, float y, float dx, float dy, const char *text)
            {
                if ((pCR == NULL) || (f.get_name() == NULL) || (text == NULL))
                    return;

                // Set current font
                font_context_t ctx;
                set_current_font(&ctx, f);
                {
                    // Output text
                    cairo_text_extents_t extents;
                    cairo_text_extents(pCR, text, &extents);

                    float r_w   = extents.x_advance - extents.x_bearing;
                    float r_h   = extents.y_advance - extents.y_bearing;
                    float fx    = x - extents.x_bearing + (r_w + 4) * 0.5f * dx - r_w * 0.5f;
                    float fy    = y - extents.y_advance + (r_h + 4) * 0.5f * (1.0f - dy) - r_h * 0.5f + 1.0f;

                    cairo_move_to(pCR, fx, fy);
                    cairo_show_text(pCR, text);
                }
                unset_current_font(&ctx);
            }

            void Win32CairoSurface::out_text_relative(const Font &f, const Color &color, float x, float y, float dx, float dy, const LSPString *text, ssize_t first, ssize_t last)
            {
                if (text == NULL)
                    return;
                out_text_relative(f, color, x, y, dx, dy, text->get_utf8(first, last));
            }

            void Win32CairoSurface::square_dot(float x, float y, float width, const Color &color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                double ow = cairo_get_line_width(pCR);
                cairo_line_cap_t cap = cairo_get_line_cap(pCR);
                setSourceRGBA(color);
                cairo_set_line_width(pCR, width);
                cairo_set_line_cap(pCR, CAIRO_LINE_CAP_SQUARE);
                cairo_move_to(pCR, x + 0.5f, y + 0.5f);
                cairo_line_to(pCR, x + 1.5f, y + 0.5f);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, ow);
                cairo_set_line_cap(pCR, cap);
            }

            void Win32CairoSurface::square_dot(float x, float y, float width, float r, float g, float b, float a)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                double ow = cairo_get_line_width(pCR);
                cairo_line_cap_t cap = cairo_get_line_cap(pCR);
                cairo_set_source_rgba(pCR, r, g, b, 1.0f - a);
                cairo_set_line_width(pCR, width);
                cairo_set_line_cap(pCR, CAIRO_LINE_CAP_SQUARE);
                cairo_move_to(pCR, x + 0.5f, y + 0.5f);
                cairo_line_to(pCR, x + 1.5f, y + 0.5f);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, ow);
                cairo_set_line_cap(pCR, cap);
            }

            void Win32CairoSurface::line(float x0, float y0, float x1, float y1, float width, const Color &color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                double ow = cairo_get_line_width(pCR);
                setSourceRGBA(color);
                cairo_set_line_width(pCR, width);
                cairo_move_to(pCR, x0, y0);
                cairo_line_to(pCR, x1, y1);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, ow);
            }

            void Win32CairoSurface::line(float x0, float y0, float x1, float y1, float width, IGradient *g)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(g);
                cg->apply(pCR);

                double ow = cairo_get_line_width(pCR);
                cairo_set_line_width(pCR, width);
                cairo_move_to(pCR, x0, y0);
                cairo_line_to(pCR, x1, y1);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, ow);
            }

            void Win32CairoSurface::parametric_line(float a, float b, float c, float width, const Color &color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                double ow = cairo_get_line_width(pCR);
                setSourceRGBA(color);
                cairo_set_line_width(pCR, width);

                if (fabs(a) > fabs(b))
                {
                    cairo_move_to(pCR, - c / a, 0.0f);
                    cairo_line_to(pCR, -(c + b*nHeight)/a, nHeight);
                }
                else
                {
                    cairo_move_to(pCR, 0.0f, - c / b);
                    cairo_line_to(pCR, nWidth, -(c + a*nWidth)/b);
                }

                cairo_stroke(pCR);
                cairo_set_line_width(pCR, ow);
            }

            void Win32CairoSurface::parametric_line(float a, float b, float c, float left, float right, float top, float bottom, float width, const Color &color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                double ow = cairo_get_line_width(pCR);
                setSourceRGBA(color);
                cairo_set_line_width(pCR, width);

                if (fabs(a) > fabs(b))
                {
                    cairo_move_to(pCR, roundf(-(c + b*top)/a), roundf(top));
                    cairo_line_to(pCR, roundf(-(c + b*bottom)/a), roundf(bottom));
                }
                else
                {
                    cairo_move_to(pCR, roundf(left), roundf(-(c + a*left)/b));
                    cairo_line_to(pCR, roundf(right), roundf(-(c + a*right)/b));
                }

                cairo_stroke(pCR);
                cairo_set_line_width(pCR, ow);
            }

            void Win32CairoSurface::parametric_bar(float a1, float b1, float c1, float a2, float b2, float c2,
                    float left, float right, float top, float bottom, IGradient *gr)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(gr);
                cg->apply(pCR);

                if (fabs(a1) > fabs(b1))
                {
                    cairo_move_to(pCR, ssize_t(-(c1 + b1*top)/a1), ssize_t(top));
                    cairo_line_to(pCR, ssize_t(-(c1 + b1*bottom)/a1), ssize_t(bottom));
                }
                else
                {
                    cairo_move_to(pCR, ssize_t(left), ssize_t(-(c1 + a1*left)/b1));
                    cairo_line_to(pCR, ssize_t(right), ssize_t(-(c1 + a1*right)/b1));
                }

                if (fabs(a2) > fabs(b2))
                {
                    cairo_line_to(pCR, ssize_t(-(c2 + b2*bottom)/a2), ssize_t(bottom));
                    cairo_line_to(pCR, ssize_t(-(c2 + b2*top)/a2), ssize_t(top));
                }
                else
                {
                    cairo_line_to(pCR, ssize_t(right), ssize_t(-(c2 + a2*right)/b2));
                    cairo_line_to(pCR, ssize_t(left), ssize_t(-(c2 + a2*left)/b2));
                }

                cairo_close_path(pCR);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::wire_arc(float x, float y, float r, float a1, float a2, float width, const Color &color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                double ow = cairo_get_line_width(pCR);
                setSourceRGBA(color);
                cairo_set_line_width(pCR, width);
                cairo_arc(pCR, x, y, r, a1, a2);
                cairo_stroke(pCR);
                cairo_set_line_width(pCR, ow);
            }

            void Win32CairoSurface::fill_poly(const Color & color, const float *x, const float *y, size_t n)
            {
                if ((pCR == NULL) || (n < 2))
                    return;

                //lsp_debug("DEBUG");

                cairo_move_to(pCR, *(x++), *(y++));
                for (size_t i=1; i < n; ++i)
                    cairo_line_to(pCR, *(x++), *(y++));

                setSourceRGBA(color);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_poly(IGradient *gr, const float *x, const float *y, size_t n)
            {
                if ((pCR == NULL) || (n < 2) || (gr == NULL))
                    return;

                //lsp_debug("DEBUG");

                cairo_move_to(pCR, *(x++), *(y++));
                for (size_t i=1; i < n; ++i)
                    cairo_line_to(pCR, *(x++), *(y++));

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(gr);
                cg->apply(pCR);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::wire_poly(const Color & color, float width, const float *x, const float *y, size_t n)
            {
                if ((pCR == NULL) || (n < 2))
                    return;

                //lsp_debug("DEBUG");

                cairo_move_to(pCR, *(x++), *(y++));
                for (size_t i=1; i < n; ++i)
                    cairo_line_to(pCR, *(x++), *(y++));

                setSourceRGBA(color);
                cairo_set_line_width(pCR, width);
                cairo_stroke(pCR);
            }

            void Win32CairoSurface::draw_poly(const Color &fill, const Color &wire, float width, const float *x, const float *y, size_t n)
            {
                if ((pCR == NULL) || (n < 2))
                    return;

                //lsp_debug("DEBUG");

                cairo_move_to(pCR, *(x++), *(y++));
                for (size_t i=1; i < n; ++i)
                    cairo_line_to(pCR, *(x++), *(y++));

                if (width > 0.0f)
                {
                    setSourceRGBA(fill);
                    cairo_fill_preserve(pCR);

                    cairo_set_line_width(pCR, width);
                    setSourceRGBA(wire);
                    cairo_stroke(pCR);
                }
                else
                {
                    setSourceRGBA(fill);
                    cairo_fill(pCR);
                }
            }

            void Win32CairoSurface::fill_circle(float x, float y, float r, const Color & color)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                setSourceRGBA(color);
                cairo_arc(pCR, x, y, r, 0.0f, M_PI * 2.0f);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_circle(float x, float y, float r, IGradient *g)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                Win32CairoGradient *cg = static_cast<Win32CairoGradient *>(g);
                cg->apply(pCR);
                cairo_arc(pCR, x, y, r, 0, M_PI * 2.0f);
                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_frame(
                const Color &color,
                float fx, float fy, float fw, float fh,
                float ix, float iy, float iw, float ih
            )
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                float fxe = fx + fw, fye = fy + fh, ixe = ix + iw, iye = iy + ih;

                if ((ix >= fxe) || (ixe < fx) || (iy >= fye) || (iye < fy))
                {
                    setSourceRGBA(color);
                    cairo_rectangle(pCR, fx, fy, fw, fh);
                    cairo_fill(pCR);
                    return;
                }
                else if ((ix <= fx) && (ixe >= fxe) && (iy <= fy) && (iye >= fye))
                    return;

                #define MOVE_TO(p, x, y) \
                    /*lsp_trace("move_to: %d, %d", int(x), int(y));*/ \
                    cairo_move_to(p, (x), (y));
                #define LINE_TO(p, x, y) \
                    /*lsp_trace("line_to: %d, %d", int(x), int(y)); */\
                    cairo_move_to(p, (x), (y));

                #define RECTANGLE(p, x, y, w, h) \
                    /*lsp_trace("rectangle: %d, %d, %d, %d", int(x), int(y), int(w), int(h)); */ \
                    cairo_rectangle(p, (x), (y), (w), (h));

                setSourceRGBA(color);
                if (ix <= fx)
                {
                    if (iy <= fy)
                    { // OK
                        RECTANGLE(pCR, ixe, fy, fxe - ixe, iye - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iye, fw, fye - iye);
                        cairo_fill(pCR);
                    }
                    else if (iye >= fye)
                    { // OK
                        RECTANGLE(pCR, fx, fy, fw, iy - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, ixe, iy, fxe - ixe, fye - iy);
                        cairo_fill(pCR);
                    }
                    else
                    { // OK
                        RECTANGLE(pCR, fx, fy, fw, iy - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, ixe, iy, fxe - ixe, ih);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iye, fw, fye - iye);
                        cairo_fill(pCR);
                    }
                }
                else if (ixe >= fxe)
                {
                    if (iy <= fy)
                    { // OK ?
                        RECTANGLE(pCR, fx, fy, ix - fx, iye - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iye, fw, fye - iye);
                        cairo_fill(pCR);
                    }
                    else if (iye >= fye)
                    { // OK
                        RECTANGLE(pCR, fx, fy, fw, iy - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iy, ix - fx, fye - iy);
                        cairo_fill(pCR);
                    }
                    else
                    { // OK
                        RECTANGLE(pCR, fx, fy, fw, iy - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iy, ix - fx, ih);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iye, fw, fye - iye);
                        cairo_fill(pCR);
                    }
                }
                else
                {
                    if (iy <= fy)
                    { // OK
                        RECTANGLE(pCR, fx, fy, ix - fx, iye - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, ixe, fy, fxe - ixe, iye - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iye, fw, fye - iye);
                        cairo_fill(pCR);
                    }
                    else if (iye >= fye)
                    { // OK
                        RECTANGLE(pCR, fx, fy, fw, iy - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iy, ix - fx, fye - iy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, ixe, iy, fxe - ixe, fye - iy);
                        cairo_fill(pCR);
                    }
                    else
                    { // OK
                        RECTANGLE(pCR, fx, fy, fw, iy - fy);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iy, ix - fx, ih);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, ixe, iy, fxe - ixe, ih);
                        cairo_fill(pCR);
                        RECTANGLE(pCR, fx, iye, fw, fye - iye);
                        cairo_fill(pCR);
                    }
                }
//                cairo_close_path(pCR);
//                cairo_fill(pCR);
            }

            void Win32CairoSurface::fill_round_frame(
                    const Color &color,
                    float radius, size_t flags,
                    float fx, float fy, float fw, float fh,
                    float ix, float iy, float iw, float ih
                    )
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                fill_frame(color, fx, fy, fw, fh, ix, iy, iw, ih);
                setSourceRGBA(color);

                // Can draw?
                float minw = 0.0f;
                minw += (flags & SURFMASK_L_CORNER) ? radius : 0.0;
                minw += (flags & SURFMASK_R_CORNER) ? radius : 0.0;
                if (iw < minw)
                    return;

                float minh = 0.0f;
                minh += (flags & SURFMASK_T_CORNER) ? radius : 0.0;
                minh += (flags & SURFMASK_B_CORNER) ? radius : 0.0;
                if (ih < minh)
                    return;

                // Draw corners
                if (flags & SURFMASK_RT_CORNER)
                {
                    cairo_move_to(pCR, ix + iw, iy);
                    cairo_line_to(pCR, ix + iw, iy + radius);
                    cairo_arc_negative(pCR, ix + iw - radius, iy + radius, radius, 2.0*M_PI, 1.5*M_PI);
                    cairo_close_path(pCR);
                    cairo_fill(pCR);
                }
                if (flags & SURFMASK_LT_CORNER)
                {
                    cairo_move_to(pCR, ix, iy);
                    cairo_line_to(pCR, ix + radius, iy);
                    cairo_arc_negative(pCR, ix + radius, iy + radius, radius, 1.5*M_PI, 1.0*M_PI);
                    cairo_close_path(pCR);
                    cairo_fill(pCR);
                }
                if (flags & SURFMASK_LB_CORNER)
                {
                    cairo_move_to(pCR, ix, iy + ih);
                    cairo_line_to(pCR, ix, iy + ih - radius);
                    cairo_arc_negative(pCR, ix + radius, iy + ih - radius, radius, 1.0*M_PI, 0.5*M_PI);
                    cairo_close_path(pCR);
                    cairo_fill(pCR);
                }
                if (flags & SURFMASK_RB_CORNER)
                {
                    cairo_move_to(pCR, ix + iw, iy + ih);
                    cairo_line_to(pCR, ix + iw - radius, iy + ih);
                    cairo_arc_negative(pCR, ix + iw - radius, iy + ih - radius, radius, 0.5*M_PI, 0.0);
                    cairo_close_path(pCR);
                    cairo_fill(pCR);
                }
            }

            bool Win32CairoSurface::get_antialiasing()
            {
                if (pCR == NULL)
                    return false;

                //lsp_debug("DEBUG");

                return cairo_get_antialias(pCR) != CAIRO_ANTIALIAS_NONE;
            }

            bool Win32CairoSurface::set_antialiasing(bool set)
            {
                if (pCR == NULL)
                    return false;

                //lsp_debug("DEBUG");

                bool old = cairo_get_antialias(pCR) != CAIRO_ANTIALIAS_NONE;
                cairo_set_antialias(pCR, (set) ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);

                return old;
            }

            surf_line_cap_t Win32CairoSurface::get_line_cap()
            {
                if (pCR == NULL)
                    return SURFLCAP_BUTT;

                cairo_line_cap_t old = cairo_get_line_cap(pCR);

                //lsp_debug("DEBUG");

                return
                    (old == CAIRO_LINE_CAP_BUTT) ? SURFLCAP_BUTT :
                    (old == CAIRO_LINE_CAP_ROUND) ? SURFLCAP_ROUND : SURFLCAP_SQUARE;
            }

            surf_line_cap_t Win32CairoSurface::set_line_cap(surf_line_cap_t lc)
            {
                if (pCR == NULL)
                    return SURFLCAP_BUTT;

                //lsp_debug("DEBUG");

                cairo_line_cap_t old = cairo_get_line_cap(pCR);

                cairo_line_cap_t cap =
                    (lc == SURFLCAP_BUTT) ? CAIRO_LINE_CAP_BUTT :
                    (lc == SURFLCAP_ROUND) ? CAIRO_LINE_CAP_ROUND :
                    CAIRO_LINE_CAP_SQUARE;

                cairo_set_line_cap(pCR, cap);

                return
                    (old == CAIRO_LINE_CAP_BUTT) ? SURFLCAP_BUTT :
                    (old == CAIRO_LINE_CAP_ROUND) ? SURFLCAP_ROUND : SURFLCAP_SQUARE;
            }

            void *Win32CairoSurface::start_direct()
            {
                if ((pCR == NULL) || (pSurface == NULL) || (nType != ST_IMAGE))
                    return NULL;

                //lsp_debug("DEBUG");

                // cairo_surface_t* offSurface = cairo_win32_surface_get_image(pSurface);
                // //bitmapData = reinterpret_cast<uint8_t *>(cairo_image_surface_get_data(offSurface));

                nStride = cairo_image_surface_get_stride(pSurface);
                return pData = reinterpret_cast<uint8_t *>(cairo_image_surface_get_data(pSurface));
            }

            void Win32CairoSurface::end_direct()
            {
                if ((pCR == NULL) || (pSurface == NULL) || (nType != ST_IMAGE) || (pData == NULL))
                    return;

                //lsp_debug("DEBUG");

                cairo_surface_mark_dirty(pSurface);
                pData = NULL;
            }

            void Win32CairoSurface::clip_begin(float x, float y, float w, float h)
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                cairo_save(pCR);
                cairo_rectangle(pCR, x, y, w, h);
                cairo_clip(pCR);
                cairo_new_path(pCR);
            }

            void Win32CairoSurface::clip_end()
            {
                if (pCR == NULL)
                    return;

                //lsp_debug("DEBUG");

                cairo_restore(pCR);
            }

        }
    }

} /* namespace lsp */
