

#include <lsp-plug.in/common/types.h>

#include <private/win32/Win32CairoGradient.h>

namespace lsp
{
    namespace ws
    {
        namespace win32
        {
            Win32CairoGradient::Win32CairoGradient()
            {
                pCP = NULL;
            }

            Win32CairoGradient::~Win32CairoGradient()
            {
                if (pCP != NULL)
                {
                    cairo_pattern_destroy(pCP);
                    pCP = NULL;
                }
            }

            void Win32CairoGradient::add_color(float offset, float r, float g, float b, float a)
            {
                if (pCP == NULL)
                    return;

                cairo_pattern_add_color_stop_rgba(pCP, offset, r, g, b, 1.0f - a);
            }

            void Win32CairoGradient::apply(cairo_t *cr)
            {
                if (pCP == NULL)
                    return;
                cairo_set_source(cr, pCP);
            }

            Win32CairoLinearGradient::~Win32CairoLinearGradient()
            {
            }

            Win32CairoRadialGradient::~Win32CairoRadialGradient()
            {
            }
        }
    }
} /* namespace lsp */
