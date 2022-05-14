#ifndef _UI_WIN32_WIN32CAIROGRADIENT_H_
#define _UI_WIN32_WIN32CAIROGRADIENT_H_

#include <lsp-plug.in/ws/version.h>
#include <lsp-plug.in/common/types.h>

#include <lsp-plug.in/ws/IGradient.h>
#include <cairo/cairo.h>

namespace lsp
{
    namespace ws
    {
        namespace win32
        {
            class Win32CairoGradient: public IGradient
            {
                protected:
                    cairo_pattern_t *pCP;

                public:
                    explicit Win32CairoGradient();
                    virtual ~Win32CairoGradient();

                public:
                    void apply(cairo_t *cr);

                public:
                    virtual void add_color(float offset, float r, float g, float b, float a);
            };

            class Win32CairoLinearGradient: public Win32CairoGradient
            {
                public:
                    explicit inline Win32CairoLinearGradient(float x0, float y0, float x1, float y1)
                    {
                        pCP = ::cairo_pattern_create_linear(x0, y0, x1, y1);
                    };

                    virtual ~Win32CairoLinearGradient();
            };

            class Win32CairoRadialGradient: public Win32CairoGradient
            {
                public:
                    explicit inline Win32CairoRadialGradient(float cx0, float cy0, float r0, float cx1, float cy1, float r1)
                    {
                        pCP = ::cairo_pattern_create_radial(cx0, cy0, r0, cx1, cy1, r1);
                    };

                    virtual ~Win32CairoRadialGradient();
            };
        }
    }
} /* namespace lsp */

#endif /* UI_WIN32_WIN32CAIROGRADIENT_H_ */
