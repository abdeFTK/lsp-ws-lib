#ifndef _UI_WIN32_WIN32WINDOW_H_
#define _UI_WIN32_WIN32WINDOW_H_

#include <lsp-plug.in/ws/version.h>
#include <lsp-plug.in/common/types.h>
#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/ws/IEventHandler.h>
#include <lsp-plug.in/ws/IWindow.h>

#include <private/win32/Win32Display.h>


#include <windows.h>


// extern "C" {

//     HINSTANCE hInstance;

//     BOOL WINAPI DllMain (HINSTANCE hInst, DWORD dwReason, LPVOID lpvReserved)
//     {
//         hInstance = hInst;
//         return 1;
//     }
// }

namespace lsp
{
    namespace ws
    {
        namespace win32
        {
            class Win32Display;
            class Win32Window: public IWindow, public IEventHandler {
                
                public:

                    typedef struct btn_event_t
                    {
                        event_t         sDown;
                        event_t         sUp;
                    } btn_event_t;

                    struct ClipInfo
                    {
                        HDC dc;
                        Win32Window* parent;
                    };

                    explicit Win32Window(Win32Display *core, IEventHandler *handler, void* nativeHwnd, void* parentHwnd, bool wrapper);
                    virtual ~Win32Window();

                    virtual status_t    init();

                    virtual void        destroy();

                    void                do_destroy();

                    static BOOL CALLBACK clipChildWindowCallback (HWND hwnd, LPARAM context)
                    {
                            ClipInfo* clipInfo = (ClipInfo*) context;

                            RECT rect;
                            GetWindowRect(hwnd, &rect); 
                            //lsp_debug("clientRect points left %ld, top %ld, right %ld, bottom %ld", rect.left, rect.top, rect.right, rect.bottom);
                            HWND parent = GetParent(hwnd);
                            MapWindowPoints(HWND_DESKTOP, parent, (LPPOINT) &rect, 2);
                            //lsp_debug("clientRect mapped left %ld, top %ld, right %ld, bottom %ld", rect.left, rect.top, rect.right, rect.bottom);
                            int res = ExcludeClipRect (clipInfo->dc, rect.left, rect.top, rect.right, rect.bottom);
                            if (res == ERROR) {
                                lsp_debug("clipChildWindowCallback left %ld, top %ld, right %ld, bottom %ld", rect.left, rect.top, rect.right, rect.bottom);
                                lsp_debug("clipChildWindowCallback error");
                            }

                            return TRUE;
                    }

                    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
                    {
                        Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
                        if (window) return window->internalWindowProc(hwnd, msg, wParam, lParam);
                        return DefWindowProc(hwnd, msg, wParam, lParam);
                    }

                    const HMODULE GetCurrentModule();

                    LRESULT CALLBACK internalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

                    inline IEventHandler *get_handler() { return pHandler; }

                    virtual ISurface *get_surface();

                    virtual ssize_t left();

                    virtual ssize_t top();

                    virtual ssize_t width();

                    virtual ssize_t height();

                    virtual bool is_visible();

                    virtual void *handle();

                    virtual size_t screen();

                    virtual status_t set_caption(const char *ascii, const char *utf8);
                
                    virtual status_t handle_event(const event_t *ev);

                    inline void set_handler(IEventHandler *handler)
                    {
                        pHandler = handler;
                    }

                    virtual status_t move(ssize_t left, ssize_t top);

                    virtual status_t resize(ssize_t width, ssize_t height);

                    virtual status_t set_geometry(const rectangle_t *realize);

                    virtual status_t set_border_style(border_style_t style);

                    virtual status_t get_border_style(border_style_t *style);

                    virtual status_t get_geometry(rectangle_t *realize);

                    virtual status_t get_absolute_geometry(rectangle_t *realize);

                    virtual status_t hide();

                    virtual status_t show();

                    virtual status_t show(IWindow *over);

                    virtual status_t grab_events(grab_t group);

                    virtual status_t ungrab_events();

                    virtual status_t set_left(ssize_t left);

                    virtual status_t set_top(ssize_t top);

                    virtual ssize_t set_width(ssize_t width);

                    virtual ssize_t set_height(ssize_t height);

                    virtual status_t set_size_constraints(const size_limit_t *c);

                    virtual status_t get_size_constraints(size_limit_t *c);

                    virtual status_t check_constraints();

                    virtual status_t set_focus(bool focus);

                    virtual status_t toggle_focus();

                    virtual status_t get_caption(char *text, size_t len);

                    virtual status_t set_icon(const void *bgra, size_t width, size_t height);

                    virtual status_t get_window_actions(size_t *actions);

                    virtual status_t set_window_actions(size_t actions);

                    virtual status_t set_mouse_pointer(mouse_pointer_t ponter);

                    virtual mouse_pointer_t get_mouse_pointer();

                    virtual status_t set_class(const char *instance, const char *wclass);

                    virtual status_t set_role(const char *wrole);

                    virtual status_t set_visibility(bool visible);

                    static bool         check_click(const btn_event_t *ev);
                    static bool         check_double_click(const btn_event_t *pe, const btn_event_t *ce);

                    bool                bWrapper;
                    size_limit_t        sConstraints;
                    ISurface           *pSurface;
                    rectangle_t         sSize;
                    size_t              nActions;
                    HWND hwnd;
                    btn_event_t         vBtnEvent[3];
                
                private:

                    HDC hdc;
                    HWND hwndNative;
                    HWND hwndParent;
                    HINSTANCE hInstance;
                    border_style_t enBorderStyle;
                    bool windowVisible;
                    bool rendering;

                    void handle_wm_create();
                    void handle_wm_size(UINT left, UINT right, UINT width, UINT height);
                    void handle_wm_move();
                    void handle_wm_paint();
                    void updateLayeredWindow();
                    void handle_mouse_button(bool down, POINT pt);
                    void handle_mouse_move(bool down, POINT pt);
                    void drop_surface();
                    void handle_wm_hide();

                    void calc_constraints(rectangle_t *dst, const rectangle_t *req);
            };

        }
    }
}

#endif /* UI_WIN32_WIN32WINDOW_H_ */