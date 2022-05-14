
#include <lsp-plug.in/common/types.h>

#include <lsp-plug.in/stdlib/string.h>
#include <lsp-plug.in/common/endian.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/ws/IWindow.h>

#include <private/win32/Win32Window.h>
#include <private/win32/Win32CairoSurface.h>

#include <windowsx.h>
#include <lsp-plug.in/runtime/system.h>



static HMODULE currentModuleHandle = NULL;

namespace lsp
{
    namespace ws
    {
        namespace win32
        {

            Win32Window::Win32Window(Win32Display *core, IEventHandler *handler, void* nativeHwnd, void* parentHwnd, bool wrapper): IWindow(core, handler) 
            {
                hwndNative = (HWND)nativeHwnd;
                hwndParent = (HWND)parentHwnd;
                bWrapper = wrapper;
                // if (parentHwnd == NULL) {
                //     bWrapper = true;
                // }
                

                sSize.nLeft             = 0;
                sSize.nTop              = 0;
                sSize.nWidth            = 32;
                sSize.nHeight           = 32;

                sConstraints.nMinWidth  = -1;
                sConstraints.nMinHeight = -1;
                sConstraints.nMaxWidth  = -1;
                sConstraints.nMaxHeight = -1;
                sConstraints.nPreWidth  = -1;
                sConstraints.nPreHeight = -1;

                enBorderStyle           = BS_SIZEABLE;
                nActions                = ws::WA_ALL;
                
                for (size_t i=0; i<3; ++i)
                {
                    init_event(&vBtnEvent[i].sDown);
                    init_event(&vBtnEvent[i].sUp);
                    vBtnEvent[i].sDown.nType    = UIE_UNKNOWN;
                    vBtnEvent[i].sUp.nType      = UIE_UNKNOWN;
                }
            }

            Win32Window::~Win32Window()
            {
                do_destroy();
            }

            const HMODULE Win32Window::GetCurrentModule()
            {
                if (currentModuleHandle == NULL)
                {
                    BOOL status = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                    (LPCTSTR) &currentModuleHandle,
                                                    &currentModuleHandle);
                }
                return currentModuleHandle;
            }

            LRESULT CALLBACK Win32Window::internalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
            {
                //lsp_debug("internalWindowProc : %d", uMsg);
                if (this->hwnd != hwnd || this->hwnd == NULL) {
                    return DefWindowProc(hwnd, uMsg, wParam, lParam);
                }
                switch (uMsg)
                {
                case WM_DESTROY:
                    {
                        //lsp_debug("WM_DESTROY");
                        do_destroy();
                        return 0;
                    }
                case WM_SHOWWINDOW:
                    {
                        if (wParam) {
                            handle_wm_create();
                        } else {
                            handle_wm_hide();
                        }
                        return 0;
                    }
                case WM_ERASEBKGND:
                    {
                        return 1;
                    }
                case WM_SYNCPAINT:
                    {
                        return 0;
                    }
                case WM_WINDOWPOSCHANGED:
                    {
                        WINDOWPOS* windowPos = (WINDOWPOS*)lParam;
                        UINT left = windowPos->x;
                        UINT right = windowPos->y;
                        UINT width = windowPos->cx;
                        UINT height = windowPos->cy;
                        handle_wm_size(left, right, width, height);
                        return 0;
                    }
                case WM_MOVE:
                    {
                        return 0;
                    }
                case WM_SIZE:
                    {
                        return 0;
                    }
                case WM_NCPAINT:
                    {
                        if (hwndNative != NULL) { // || hwndParent != NULL) {
                            handle_wm_paint();
                            return 0;
                        } else if (hwndParent != NULL) {
                            RECT rect;
                            RECT clipRect;
                            int border = 1;
                            GetWindowRect(hwnd, &rect);
                            CopyRect(&clipRect, &rect);
                            // Resize clip rect
                            InflateRect(&clipRect, -border, -border);
                            // Map points from screen space to window space
                            MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT) &rect, 2);
                            MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT) &clipRect, 2);
                            HDC hdc;
                            hdc = GetDC(hwnd);
                            SetMapMode (hdc, MM_TEXT);
                            COLORREF rgbBlack = 0x00000000;
                            HBRUSH hbr = CreateSolidBrush(rgbBlack);
                            // Clip inner area
                            ExcludeClipRect (hdc, clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);
                            // Draw border
                            FillRect(hdc, &rect, hbr);
                            DeleteObject(hbr);
                            ReleaseDC(hwnd, hdc);
                            return 0;
                        }
                        break;
                    }
                case WM_PAINT:
                    {
                        handle_wm_paint();
                        return 0;
                    }
                case WM_LBUTTONDOWN:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam);
                        handle_mouse_button(true, pt);
                        return 0;
                    }
                case WM_LBUTTONUP:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam);
                        handle_mouse_button(false, pt);
                        return 0;
                    }
                case WM_MOUSEMOVE:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam); 
                        pt.y = GET_Y_LPARAM(lParam); 
                        handle_mouse_move(false, pt);
                        return 0;
                    }
                }
                
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
            }

            status_t Win32Window::init()
            {
                windowVisible = false;
                rendering = false;
                static_cast<Win32Display *>(pDisplay)->add_window(this);
                if (bWrapper) {
                    //lsp_debug("Init Wrapper Window");
                    return STATUS_OK;
                }

                // Register the window class.
                const char CLASS_NAME[]  = "LSP_Window";
                hInstance = GetCurrentModule();
                WNDCLASSEXA wc = { };
                wc.cbSize = sizeof(WNDCLASSEXA);
                wc.lpfnWndProc   = WndProc;
                wc.hInstance     = hInstance;
                wc.lpszClassName = CLASS_NAME;
                wc.style = 0;
                wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND+1;

                // if (hInstance == NULL)
                //     lsp_debug("MAIN hInstance is NULL");

                DWORD dwStyle = WS_CHILD;
                DWORD dwExStyle = 0;

                HWND ownerWnd = NULL;

                if (hwndNative != NULL) {
                    //lsp_debug("ROOT window is NOT NULL");
                    dwStyle = WS_CHILD;
                    ownerWnd = hwndNative;
                    ATOM registerRes = RegisterClassExA(&wc);
                    if (registerRes == 0) {
                        DWORD errorReg = GetLastError();
                        //lsp_debug("RegisterClass LAST ERROR %d", errorReg);
                    }
                } else if (hwndParent != NULL) {
                    //lsp_debug("PARENT window is NOT NULL");
                    dwStyle = WS_CHILD;
                    ownerWnd = hwndParent;
                } else {
                    //lsp_debug("No ROOT or PARENT window");
                    dwStyle = WS_OVERLAPPEDWINDOW;
                    ownerWnd = NULL;
                }

                // Calculate window constraints
                calc_constraints(&sSize, &sSize);

                // Create the window.
                hwnd = CreateWindowExA(
                    dwExStyle,          // Optional window styles.
                    CLASS_NAME,         // Window class
                    "LSP Plugin",       // Window text
                    dwStyle,            // Window style

                    // Size and position
                    0, 0, 100, 100,

                    ownerWnd,    // Parent window    
                    NULL,        // Menu
                    hInstance,   // Instance handle
                    NULL         // Additional application data
                    );

                if (hwnd == NULL) {
                    DWORD errorwnd = GetLastError();
                    //lsp_debug("CreateWindowExA LAST ERROR %ld", errorwnd);
                } else {
                    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR) this);
                    pSurface = new Win32CairoSurface(static_cast<Win32Display *>(pDisplay), hwnd, 100, 100);
                    lsp_debug("INIT surface created");
                }

                return STATUS_OK;
            }

            void Win32Window::do_destroy() {
                //lsp_debug("DO_DESTROY");
                set_handler(NULL);
                if (hwnd != NULL) {
                    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR) 0);
                }
                hdc = NULL;
                hwnd = NULL;
                hwndNative = NULL;
                hwndParent = NULL;
                pDisplay = NULL;
            }

            void Win32Window::destroy()
            {
               // lsp_debug("DESTROY");
                // Drop surface
                if (hwnd != NULL) {
                    SetLastError(0);
                    int res = SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR) 0);
                    if (res == 0) {
                        DWORD err = GetLastError();
                        //lsp_debug("SetWindowLongPtrA error %d", err);
                    }
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW);
                }
                hide();
                drop_surface();
                
                if (hwnd != NULL) {
                    //ReleaseDC(hwnd, hdc);
                    if (DestroyWindow(hwnd) == 0) {
                        DWORD res = GetLastError();
                        //lsp_debug("DestroyWindow error %d", res);
                    }
                }

                if (pDisplay != NULL) {
                    static_cast<Win32Display *>(pDisplay)->remove_window(this);
                }

                do_destroy();
            }

            void Win32Window::drop_surface()
            {
                if (pSurface != NULL)
                {
                    pSurface->destroy();
                    delete pSurface;
                    pSurface = NULL;
                }
                windowVisible = false;
            }

            void Win32Window::calc_constraints(rectangle_t *dst, const rectangle_t *req)
            {
                *dst    = *req;

                if ((sConstraints.nMaxWidth >= 0) && (dst->nWidth > sConstraints.nMaxWidth))
                    dst->nWidth         = sConstraints.nMaxWidth;
                if ((sConstraints.nMaxHeight >= 0) && (dst->nHeight > sConstraints.nMaxHeight))
                    dst->nHeight        = sConstraints.nMaxHeight;
                if ((sConstraints.nMinWidth >= 0) && (dst->nWidth < sConstraints.nMinWidth))
                    dst->nWidth         = sConstraints.nMinWidth;
                if ((sConstraints.nMinHeight >= 0) && (dst->nHeight < sConstraints.nMinHeight))
                    dst->nHeight        = sConstraints.nMinHeight;
            }

            ISurface *Win32Window::get_surface()
            {
                if (bWrapper || rendering || !windowVisible)
                    return NULL;
                return pSurface;
            }

            ssize_t Win32Window::left()
            {
                return sSize.nLeft;
            }

            ssize_t Win32Window::top()
            {
                return sSize.nTop;
            }

            ssize_t Win32Window::width()
            {
                return sSize.nWidth;
            }

            ssize_t Win32Window::height()
            {
                return sSize.nHeight;
            }

            bool Win32Window::is_visible()
            {
                return pSurface != NULL && windowVisible;
            }

            status_t Win32Window::set_visibility(bool visible)
            {
                //lsp_debug("set_visibility %d", visible);
                return IWindow::set_visibility(visible);
            }

            size_t Win32Window::screen()
            {
                return 1;
            }

            status_t Win32Window::set_caption(const char *ascii, const char *utf8)
            {

                return STATUS_OK;
            }

            void *Win32Window::handle()
            {
                //lsp_debug("Window get hwnd");
                return this->hwnd;
            }

            status_t Win32Window::handle_event(const event_t *ev) {
                IEventHandler *handler = pHandler;
                if (handler != NULL)
                {
                    handler->handle_event(ev);
                }
                return STATUS_OK;
            } 

            void Win32Window::handle_wm_move()
            {
                //lsp_debug("WM_MOVE");
                event_t ue;
                init_event(&ue);
                ue.nType        = UIE_REDRAW;
                ue.nLeft        = 0;
                ue.nTop         = 0;
                ue.nWidth       = sSize.nWidth;
                ue.nHeight      = sSize.nHeight;
                
                if (!bWrapper) {
                    if (pSurface != NULL) {
                        static_cast<Win32CairoSurface *>(pSurface)->resize(sSize.nWidth, sSize.nHeight);
                    }
                }

                handle_event(&ue);
            }

            void Win32Window::handle_mouse_move(bool down, POINT pt) {
                size_t nState = 0;
                if (GetCapture() == hwnd) {
                    //printf("mouse move x %ld, y %ld\n", pt.x, pt.y);
                    nState = MCF_LEFT;
                }
                event_t ue;
                ue.nType        = UIE_MOUSE_MOVE;
                ue.nLeft        = pt.x;
                ue.nTop         = pt.y;
                ue.nState       = nState;
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                ue.nTime        = xts;
                handle_event(&ue);
            }

            void Win32Window::handle_mouse_button(bool down, POINT pt) {

                event_t ue;
                init_event(&ue);
                ue.nCode        = MCB_LEFT;
                ue.nType        = down ? UIE_MOUSE_DOWN : UIE_MOUSE_UP;
                ue.nLeft        = pt.x;
                ue.nTop         = pt.y;
                ue.nState       = MCF_LEFT;
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                ue.nTime        = xts;

                // Additionally generated event
                event_t gen;
                gen = ue;
                gen.nType       = UIE_UNKNOWN;

                HWND lockedWindow = GetCapture();
                if (down) {
                    //lsp_debug("Capture mouse");
                    SetCapture(hwnd);
                    // Shift the buffer and push event
                    vBtnEvent[0]            = vBtnEvent[1];
                    vBtnEvent[1]            = vBtnEvent[2];
                    vBtnEvent[2].sDown      = ue;
                    init_event(&vBtnEvent[2].sUp);
                } else {
                    if (hwnd == lockedWindow) {
                        //lsp_debug("Release mouse capture");
                        ReleaseCapture();
                    }
                    // Push event
                    vBtnEvent[2].sUp        = ue;
                    if (check_click(&vBtnEvent[2]))
                    {
                        gen.nType               = UIE_MOUSE_CLICK; // Crash on popup menu close : Bug ?
                        if (check_double_click(&vBtnEvent[1], &vBtnEvent[2]))
                        {
                            gen.nType               = UIE_MOUSE_DBL_CLICK;
                            if (check_double_click(&vBtnEvent[0], &vBtnEvent[1]))
                                gen.nType             = UIE_MOUSE_TRI_CLICK;
                        }
                    }
                }  
                IEventHandler *handler = pHandler;
                if (handler != NULL) {
                    handler->handle_event(&ue);
                    // !!!!!! "this" can be deleted at this time !!!!!!
                    // don't call member functions
                    if (gen.nType != UIE_UNKNOWN) {
                        handler->handle_event(&gen);
                    }
                }  
            }

            void Win32Window::handle_wm_paint() {
                if (pSurface != NULL && windowVisible) {
                    rendering = true;
                    PAINTSTRUCT paintStruct;
                    HDC dc = BeginPaint (hwnd, &paintStruct);
                    SetMapMode (dc, MM_TEXT);
                    ClipInfo clipInfo = { dc, this };
                    EnumChildWindows (hwnd, clipChildWindowCallback, (LPARAM) &clipInfo);
                    if (hwndParent != NULL) {
                        RECT rect;
                        GetWindowRect(hwnd, &rect);
                        int border = 1;
                        InflateRect(&rect, -border, -border);
                        MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT) &rect, 2);
                        IntersectClipRect(dc, rect.left, rect.top, rect.right, rect.bottom);
                    }
                    static_cast<Win32CairoSurface *>(pSurface)->renderOffscreen(dc, paintStruct, sSize.nWidth, sSize.nHeight);
                    EndPaint (hwnd, &paintStruct);
                    rendering = false;
                }
            }

            void Win32Window::handle_wm_size(UINT left, UINT right, UINT width, UINT height)
            {
                //lsp_debug("WM_SIZE x %d, y %d width %d, height %d", left, right, width, height);
                event_t ue;
                init_event(&ue);
                ue.nType        = UIE_RESIZE;
                ue.nLeft        = left;
                ue.nTop         = right;
                ue.nWidth       = width;
                ue.nHeight      = height;
                
                if (!bWrapper) { 
                    if (pSurface != NULL) {
                        static_cast<Win32CairoSurface *>(pSurface)->resize(sSize.nWidth, sSize.nHeight);
                    }
                }
                handle_event(&ue);
            }

            void Win32Window::handle_wm_hide() {
                //lsp_debug("WM_HIDE");
                event_t ue;
                init_event(&ue);
                ue.nType        = UIE_HIDE;

                windowVisible = false;
                drop_surface();

                handle_event(&ue);
            }

            void Win32Window::handle_wm_create()
            {
                //lsp_debug("WM_SHOW");
                event_t ue;
                init_event(&ue);
                ue.nType        = UIE_SHOW;

                windowVisible = true;
                
                if (pSurface != NULL) {
                    static_cast<Win32CairoSurface *>(pSurface)->resize(sSize.nWidth, sSize.nHeight);
                }
                handle_event(&ue);
            }

            void Win32Window::updateLayeredWindow() {
            }

            status_t Win32Window::move(ssize_t left, ssize_t top)
            {
                //lsp_debug("left : %ld, top : %ld", left, top);
                sSize.nLeft    = left;
                sSize.nTop   = top;
                if (!bWrapper) {
                    MoveWindow(hwnd,sSize.nLeft,sSize.nTop,sSize.nWidth,sSize.nHeight,TRUE);
                }
                return STATUS_OK;
            }

            status_t Win32Window::resize(ssize_t width, ssize_t height)
            {
                //lsp_debug("width : %ld, height : %ld", width, height);
                sSize.nWidth    = width;
                sSize.nHeight   = height;
                if (!bWrapper) {
                    //SetWindowPos(hwnd, NULL,sSize.nLeft,sSize.nTop,sSize.nWidth,sSize.nHeight,SWP_NOREDRAW | SWP_NOSENDCHANGING);
                    MoveWindow(hwnd,sSize.nLeft,sSize.nTop,sSize.nWidth,sSize.nHeight,TRUE);
                }
                return STATUS_OK;
            }

            status_t Win32Window::set_geometry(const rectangle_t *realize)
            {
                rectangle_t old = sSize;
                calc_constraints(&sSize, realize);

                if ((old.nLeft == sSize.nLeft) &&
                    (old.nTop == sSize.nTop) &&
                    (old.nWidth == sSize.nWidth) &&
                    (old.nHeight == sSize.nHeight))
                    return STATUS_OK;

                //lsp_debug("left=%d, top=%d, width=%d, height=%d", int(sSize.nLeft), int(sSize.nTop), int(sSize.nWidth), int(sSize.nHeight));
                //MoveWindow(hwnd,sSize.nLeft,sSize.nTop,sSize.nWidth,sSize.nHeight,FALSE);
                SetWindowPos(hwnd, NULL, sSize.nLeft, sSize.nTop, sSize.nWidth, sSize.nHeight, SWP_NOREDRAW | SWP_NOSENDCHANGING);


                // if (hwndParent != NULL)
                // {
                //     if ((old.nWidth == sSize.nWidth) &&
                //         (old.nHeight == sSize.nHeight))
                //         return STATUS_OK;

                //     //SetWindowPos(hwnd, NULL, 0, 0, sSize.nWidth, sSize.nHeight, SWP_NOREDRAW | SWP_NOSENDCHANGING);
                //     MoveWindow(hwnd,sSize.nLeft,sSize.nTop,sSize.nWidth,sSize.nHeight,FALSE);
                // }
                // else
                // {
                //     if ((old.nLeft == sSize.nLeft) &&
                //         (old.nTop == sSize.nTop) &&
                //         (old.nWidth == sSize.nWidth) &&
                //         (old.nHeight == sSize.nHeight))
                //         return STATUS_OK;

                //     //SetWindowPos(hwnd, NULL,sSize.nLeft,sSize.nTop,sSize.nWidth,sSize.nHeight,SWP_NOREDRAW | SWP_NOSENDCHANGING);
                //     MoveWindow(hwnd,sSize.nLeft,sSize.nTop,sSize.nWidth,sSize.nHeight,FALSE);
                // }

                return STATUS_OK;
            }

            status_t Win32Window::set_border_style(border_style_t style)
            {
                enBorderStyle = style;
                //lsp_debug("set border style");
                return STATUS_OK;
            }

            status_t Win32Window::get_border_style(border_style_t *style)
            {
                if (style != NULL)
                    *style = enBorderStyle;
                return STATUS_OK;
            }

            status_t Win32Window::get_geometry(rectangle_t *realize)
            {
                if (realize != NULL)
                    *realize    = sSize;
                return STATUS_OK;
            }

            status_t Win32Window::get_absolute_geometry(rectangle_t *realize)
            {
                if (realize != NULL) {
                    realize->nLeft      = sSize.nLeft;
                    realize->nTop       = sSize.nTop;
                    realize->nWidth     = sSize.nWidth;
                    realize->nHeight    = sSize.nHeight;
                }
                return STATUS_OK;
            }

            status_t Win32Window::get_caption(char *text, size_t len)
            {
                return STATUS_OK;
            }

            status_t Win32Window::hide()
            {
                windowVisible = false;
                if (hwnd == NULL) {
                    return STATUS_BAD_STATE;
                }
                if (hwnd != NULL && pSurface != NULL) {
                    ShowWindow(this->hwnd, SW_HIDE);
                }
                return STATUS_OK;
            }

            status_t Win32Window::show()
            {
                return show(NULL);
            }

            status_t Win32Window::show(IWindow *over)
            {
                if (windowVisible)
                    return STATUS_OK;
                if (hwnd == NULL) {
                    //lsp_debug("BAD STATE");
                    return STATUS_BAD_STATE;
                }
                ShowWindow(this->hwnd, SW_SHOWNORMAL);
                return STATUS_OK;
            }

            status_t Win32Window::set_left(ssize_t left)
            {
                return move(left, sSize.nTop);
            }

            status_t Win32Window::set_top(ssize_t top)
            {
                return move(sSize.nLeft, top);
            }

            ssize_t Win32Window::set_width(ssize_t width)
            {
                return resize(width, sSize.nHeight);
            }

            ssize_t Win32Window::set_height(ssize_t height)
            {
                return resize(sSize.nWidth, height);
            }

            status_t Win32Window::set_size_constraints(const size_limit_t *c)
            {
                sConstraints    = *c;
                if (sConstraints.nMinWidth == 0)
                    sConstraints.nMinWidth  = 1;
                if (sConstraints.nMinHeight == 0)
                    sConstraints.nMinHeight = 1;

                calc_constraints(&sSize, &sSize);
                //SetWindowPos(hwnd, NULL, 0, 0, sSize.nWidth, sSize.nHeight, SWP_NOREDRAW | SWP_NOSENDCHANGING);
                MoveWindow(hwnd,sSize.nLeft,sSize.nTop,sSize.nWidth,sSize.nHeight,TRUE);
                return STATUS_OK;
            }

            status_t Win32Window::get_size_constraints(size_limit_t *c)
            {
                *c = sConstraints;
                return STATUS_OK;
            }

            status_t Win32Window::check_constraints()
            {
                rectangle_t rs;

                calc_constraints(&rs, &sSize);
                if ((rs.nWidth == sSize.nWidth) && (rs.nHeight == sSize.nHeight))
                    return STATUS_OK;

                //lsp_debug("width=%d, height=%d", int(sSize.nWidth), int(sSize.nHeight));

                //SetWindowPos(hwnd, NULL, 0, 0, sSize.nWidth, sSize.nHeight, SWP_NOREDRAW | SWP_NOSENDCHANGING);
                MoveWindow(hwnd,sSize.nLeft,sSize.nTop,sSize.nWidth,sSize.nHeight,TRUE);
                return STATUS_OK;
            }

            status_t Win32Window::set_focus(bool focus)
            {
                return STATUS_OK;
            }

            status_t Win32Window::toggle_focus()
            {
                return STATUS_OK;
            }

            status_t Win32Window::set_icon(const void *bgra, size_t width, size_t height)
            {
                return STATUS_OK;
            }

            status_t Win32Window::get_window_actions(size_t *actions)
            {
                if (actions == NULL)
                    return STATUS_BAD_ARGUMENTS;
                *actions = nActions;
                return STATUS_OK;
            }

            status_t Win32Window::set_window_actions(size_t actions)
            {
                return STATUS_OK;
            }

            status_t Win32Window::set_mouse_pointer(mouse_pointer_t pointer)
            {
                return STATUS_OK;
            }

            mouse_pointer_t Win32Window::get_mouse_pointer()
            {
                return MP_ARROW;
            }

            status_t Win32Window::grab_events(grab_t group)
            {
               switch (group)
                {
                    case GRAB_DROPDOWN:
                    case GRAB_MENU:
                    case GRAB_EXTRA_MENU:
                        //SetCapture(hwnd);
                        break;
                    default:
                        break;
                }
                return STATUS_OK;
            }

            status_t Win32Window::ungrab_events()
            {
                HWND grabbedWindow = GetCapture();
                if (grabbedWindow == hwnd) {
                    //ReleaseCapture();
                }
                return STATUS_OK;
            }

            bool Win32Window::check_click(const btn_event_t *ev)
            {
                if ((ev->sDown.nType != UIE_MOUSE_DOWN) || (ev->sUp.nType != UIE_MOUSE_UP))
                    return false;
                if (ev->sDown.nCode != ev->sUp.nCode)
                    return false;
                if ((ev->sUp.nTime < ev->sDown.nTime) || ((ev->sUp.nTime - ev->sDown.nTime) > 400))
                    return false;

                return (ev->sDown.nLeft == ev->sUp.nLeft) && (ev->sDown.nTop == ev->sUp.nTop);
            }

            bool Win32Window::check_double_click(const btn_event_t *pe, const btn_event_t *ev)
            {
                if (!check_click(pe))
                    return false;

                if (pe->sDown.nCode != ev->sDown.nCode)
                    return false;
                if ((ev->sUp.nTime < pe->sUp.nTime) || ((ev->sUp.nTime - pe->sUp.nTime) > 400))
                    return false;

                return (pe->sUp.nLeft == ev->sUp.nLeft) && (pe->sUp.nTop == ev->sUp.nTop);
            }

            status_t Win32Window::set_class(const char *instance, const char *wclass)
            {
                if ((instance == NULL) || (wclass == NULL))
                    return STATUS_BAD_ARGUMENTS;

                size_t l1 = ::strlen(instance);
                size_t l2 = ::strlen(wclass);

                char *dup = reinterpret_cast<char *>(::malloc((l1 + l2 + 2) * sizeof(char)));
                if (dup == NULL)
                    return STATUS_NO_MEM;

                ::memcpy(dup, instance, l1+1);
                ::memcpy(&dup[l1+1], wclass, l2+1);

                //lsp_debug("set_class %s", dup);

                ::free(dup);
                return STATUS_OK;
            }

            status_t Win32Window::set_role(const char *wrole)
            {
                if (wrole == NULL)
                    return STATUS_BAD_ARGUMENTS;

                //lsp_debug("set_role %s", wrole);

                return STATUS_OK;
            }
        }
    }
}