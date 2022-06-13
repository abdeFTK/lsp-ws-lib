#ifndef _UI_WIN32_WIN32WINDOW_H_
#define _UI_WIN32_WIN32WINDOW_H_

#include <lsp-plug.in/ws/version.h>
#include <lsp-plug.in/common/types.h>
#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/ws/IEventHandler.h>
#include <lsp-plug.in/ws/IWindow.h>

#include <private/win32/Win32Display.h>

#include <windows.h>

namespace lsp
{
    namespace ws
    {
        namespace win32
        {
            class Win32Display;
            class Win32Window: public IWindow, public IEventHandler, public IDropTarget {
                
                public:

                    enum flags_t
                    {
                        F_GRABBING      = 1 << 0,
                        F_LOCKING       = 1 << 1
                    };

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

                    explicit Win32Window(Win32Display *core, IEventHandler *handler, void* nativeHwnd, void* parentHwnd, void* ownerHwnd, size_t screen, bool wrapper, HINSTANCE hIn, const wchar_t* wcName);
                    virtual ~Win32Window();

                    virtual status_t    init();

                    virtual void        destroy();

                    void                do_destroy();

                    static BOOL CALLBACK clipChildWindowCallback (HWND hwnd, LPARAM context);

                    int internalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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

                    virtual bool has_parent() const;

                    virtual status_t set_visibility(bool visible);

                    static bool         check_click(const btn_event_t *ev);
                    static bool         check_double_click(const btn_event_t *pe, const btn_event_t *ce);

                    bool                bWrapper;
                    size_limit_t        sConstraints;
                    ISurface           *pSurface;
                    rectangle_t         sSize;
                    size_t              nActions;
                    HWND hwnd;
                    HWND hwndNative;
                    btn_event_t         vBtnEvent[3];

                    void handle_wm_size(UINT left, UINT right, UINT width, UINT height);
                    void handle_wm_paint();

                    HRESULT DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

                    HRESULT DragLeave();

                    HRESULT DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

                    HRESULT Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

                    ULONG AddRef();

                    ULONG Release();

                    HRESULT QueryInterface(REFIID riid, void   **ppvObject);
                    
                private:

                    HDC hdc;
                    
                    HWND hwndParent;
                    HWND hwndOwner;
                    HINSTANCE hInstance;
                    const wchar_t* wndClsName;
                    
                    border_style_t enBorderStyle;
                    bool windowVisible;
                    bool rendering;
                    TRACKMOUSEEVENT tme;
                    size_t              nFlags;
                    bool isTracking;
                    size_t              nScreen;
                    mouse_pointer_t     enPointer;

                    void handle_wm_show();
                    
                    
                    void handle_key(ui_event_type_t type, size_t vKey);
                    void handle_char(ui_event_type_t type, size_t charCode);
                    void handle_mouse_button(code_t button, size_t type, POINT pt, size_t state);
                    void handle_mouse_move(POINT pt, size_t state);
                    void handle_mouse_scroll(int delta, POINT pt, size_t state);
                    void handle_mouse_hover(bool enter, POINT pt);
                    void drop_surface();
                    void handle_wm_hide();
                    void handle_wm_focus(bool focused);
                    void handle_wm_dropfiles(HDROP hdrop);

                    void calc_constraints(rectangle_t *dst, const rectangle_t *req);

                    void setWndPos();
            };

        }
    }
}

#endif /* UI_WIN32_WIN32WINDOW_H_ */