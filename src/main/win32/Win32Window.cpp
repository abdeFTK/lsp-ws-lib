
#include <lsp-plug.in/common/types.h>

#include <lsp-plug.in/stdlib/string.h>
#include <lsp-plug.in/common/endian.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/ws/IWindow.h>
#include <lsp-plug.in/ipc/Thread.h>

#include <private/win32/Win32Window.h>
#include <private/win32/Win32CairoSurface.h>
#include <private/win32/decode.h>
#include <lsp-plug.in/ws/keycodes.h>

#include <windowsx.h>
#include <lsp-plug.in/runtime/system.h>
#include <lsp-plug.in/io/charset.h>

#define CHILD_BORDER_WIDTH 1
#define CHILD_BORDER_COLOR 0x00000000

// True while the window is being resized from the bottom right corner
static bool inMouseResize = false;

namespace lsp
{
    namespace ws
    {
        namespace win32
        {

            Win32Window::Win32Window(Win32Display *core, IEventHandler *handler, void* nativeHwnd, void* parentHwnd, void* ownerHwnd, size_t screen, bool wrapper, HINSTANCE hIn, const wchar_t* wcName): IWindow(core, handler) 
            {
                hwndNative = (HWND)nativeHwnd;
                hwndParent = (HWND)parentHwnd;
                hwndOwner = (HWND)ownerHwnd;
                hInstance = hIn;
                wndClsName = wcName;
                bWrapper = wrapper;
                nScreen = screen;

                sSize.nLeft             = 0;
                sSize.nTop              = 0;
                sSize.nWidth            = 32;
                sSize.nHeight           = 32;

                rootWndPos.x            = 0;
                rootWndPos.y            = 0;

                sConstraints.nMinWidth  = -1;
                sConstraints.nMinHeight = -1;
                sConstraints.nMaxWidth  = -1;
                sConstraints.nMaxHeight = -1;
                sConstraints.nPreWidth  = -1;
                sConstraints.nPreHeight = -1;

                pSurface = NULL;

                enBorderStyle           = BS_SIZEABLE;
                nActions                = ws::WA_ALL;
                nFlags                  = 0;
                enPointer               = MP_DEFAULT;
                isTracking = false;
                
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

            int Win32Window::internalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
            {
                if (this->hwnd != hwnd || this->hwnd == NULL) {
                    return -1;
                }
                switch (uMsg)
                {
                case WM_CLOSE:
                    {
                        event_t ue;
                        init_event(&ue);
                        ue.nType        = UIE_CLOSE;
                        handle_event(&ue);
                        return 0;
                    }
                case WM_DESTROY:
                    {
                        do_destroy();
                        return 0;
                    }
                case WM_SHOWWINDOW:
                    {
                        if (wParam) {
                            handle_wm_show();
                            set_focus(true);
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
                        RECT rect;
                        WINDOWPOS* windowPos = (WINDOWPOS*)lParam;
                        
                        rootWndPos.x = windowPos->x;
                        rootWndPos.y = windowPos->y;

                        if (hwndParent == NULL) {
                            GetClientRect(hwnd, &rect);
                        } else {
                            SetRect(&rect, windowPos->x, windowPos->y, windowPos->cx, windowPos->cy);
                            InflateRect(&rect, -CHILD_BORDER_WIDTH, -CHILD_BORDER_WIDTH);
                        }

                        handle_wm_size(rect.left, rect.top, rect.right, rect.bottom);
                        return 0;
                    }
                case WM_GETMINMAXINFO:
                    {
                        WINDOWINFO wndInfo;
                        wndInfo.cbSize = sizeof(WINDOWINFO);
                        GetWindowInfo(hwnd, &wndInfo);

                        UINT extraWidth = (wndInfo.rcClient.left - wndInfo.rcWindow.left) + (wndInfo.rcWindow.right - wndInfo.rcClient.right);
                        UINT extraHeight = (wndInfo.rcClient.top - wndInfo.rcWindow.top) + (wndInfo.rcWindow.bottom - wndInfo.rcClient.bottom); 

                        POINT minSize;
                        minSize.x = sConstraints.nMinWidth + extraWidth;
                        minSize.y = sConstraints.nMinHeight + extraHeight;
                        MINMAXINFO* minMaxInfo = (MINMAXINFO*)lParam;
                        minMaxInfo->ptMinTrackSize = minSize;
                        break;
                    }
                case WM_MOVE:
                    {
                        return 0;
                    }
                case WM_SIZE:
                    {
                        return 0;
                    }
                case WM_NCCALCSIZE:
                    {
                        if (hwndParent != NULL) {
                            if (wParam) {
                                NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lParam;
                                CopyRect(&params->rgrc[1], &params->rgrc[0]);
                                CopyRect(&params->rgrc[2], &params->rgrc[0]);
                                params->rgrc[0].left += CHILD_BORDER_WIDTH;
                                params->rgrc[0].top += CHILD_BORDER_WIDTH;
                                InflateRect(&params->rgrc[1], CHILD_BORDER_WIDTH , CHILD_BORDER_WIDTH );
                                InflateRect(&params->rgrc[2], CHILD_BORDER_WIDTH , CHILD_BORDER_WIDTH );
                                return WVR_VALIDRECTS;
                            }
                        }
                        break;
                    }
                case WM_NCPAINT:
                    {
                        if (hwndParent != NULL) {
                            RECT rect;
                            RECT clipRect;
                            GetClientRect(hwnd, &clipRect);
                            GetWindowRect(hwnd, &rect);
                            rect.right += CHILD_BORDER_WIDTH + 1;
                            rect.bottom += CHILD_BORDER_WIDTH + 1;
                            // Map points from screen space to window space
                            MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT) &rect, 2);
                            HDC hdc;
                            hdc = GetWindowDC(hwnd);
                            SetMapMode (hdc, MM_TEXT);
                            COLORREF rgbBlack = CHILD_BORDER_COLOR;
                            HBRUSH hbr = CreateSolidBrush(rgbBlack);
                            // Clip inner area
                            ExcludeClipRect (hdc, CHILD_BORDER_WIDTH, 
                                CHILD_BORDER_WIDTH, 
                                clipRect.right + 1, 
                                clipRect.bottom + 1);
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
                case WM_MOUSELEAVE:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam);
                        if (isTracking) {
                            isTracking = false;
                            handle_mouse_hover(false, pt);
                            return 0;
                        }
                    }
                case WM_MOUSEWHEEL:
                    {
                        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam); 
                        size_t vKey = GET_KEYSTATE_WPARAM(wParam);
                        size_t state = decode_mouse_state(vKey);
                        MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT) &pt, 1);
                        handle_mouse_scroll(zDelta, pt, state);
                        return 0;
                    }
                case WM_LBUTTONDOWN:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam);

                        if (hwndNative != NULL) {
                            RECT rect;
                            GetClientRect(hwnd, &rect);
                            if (pt.x > rect.right - 10 && pt.y > rect.bottom - 10) {
                                SetCapture(hwnd);
                                HANDLE cur = static_cast<Win32Display *>(pDisplay)->get_cursor(MP_SIZE_NWSE);
                                if (cur != NULL)
                                    SetCursor((HCURSOR)cur);
                                inMouseResize = true;
                            }
                        }

                        size_t vKey = wParam;
                        size_t state = decode_mouse_state(vKey);
                        handle_mouse_button(MCB_LEFT, UIE_MOUSE_DOWN, pt, state);
                        return 0;
                    }
                case WM_LBUTTONUP:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam);

                        if (inMouseResize) {
                            HANDLE cur = static_cast<Win32Display *>(pDisplay)->get_cursor(MP_ARROW);
                            if (cur != NULL)
                                SetCursor((HCURSOR)cur);
                        }
                        inMouseResize = false;

                        size_t vKey = wParam;
                        size_t state = decode_mouse_state(vKey);
                        handle_mouse_button(MCB_LEFT, UIE_MOUSE_UP, pt, state);
                        return 0;
                    }
                case WM_CAPTURECHANGED:
                    {
                        if (hwndNative != NULL) {
                             inMouseResize = false;
                        }
                        break;
                    }
                case WM_RBUTTONDOWN:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam);
                        size_t vKey = wParam;
                        size_t state = decode_mouse_state(vKey);
                        handle_mouse_button(MCB_RIGHT, UIE_MOUSE_DOWN, pt, state);
                        return 0;
                    }
                case WM_RBUTTONUP:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam);
                        size_t vKey = wParam;
                        size_t state = decode_mouse_state(vKey);
                        handle_mouse_button(MCB_RIGHT, UIE_MOUSE_UP, pt, state);
                        return 0;
                    }
                case WM_MBUTTONDOWN:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam);
                        size_t vKey = wParam;
                        size_t state = decode_mouse_state(vKey);
                        handle_mouse_button(MCB_MIDDLE, UIE_MOUSE_DOWN, pt, state);
                        return 0;
                    }
                case WM_MBUTTONUP:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam);
                        pt.y = GET_Y_LPARAM(lParam);
                        size_t vKey = wParam;
                        size_t state = decode_mouse_state(vKey);
                        handle_mouse_button(MCB_MIDDLE, UIE_MOUSE_UP, pt, state);
                        return 0;
                    }
                case WM_MOUSEMOVE:
                    {
                        POINT pt;
                        pt.x = GET_X_LPARAM(lParam); 
                        pt.y = GET_Y_LPARAM(lParam); 

                        if (hwndNative != NULL) {
                            RECT rect;
                            GetClientRect(hwnd, &rect);
                            if (inMouseResize && GetCapture() == hwnd) {
                                long minWidth = sConstraints.nMinWidth;
                                long minHeight = sConstraints.nMinHeight;
                                int width = lsp_max(minWidth, pt.x); 
                                int height = lsp_max(minHeight, pt.y); 
                                resize(width, height);
                                return 0;
                            } else if (pt.x > rect.right - 10 && pt.y > rect.bottom - 10) {
                                HANDLE cur = static_cast<Win32Display *>(pDisplay)->get_cursor(MP_SIZE_NWSE);
                                if (cur != NULL)
                                    SetCursor((HCURSOR)cur);
                                return 0;
                            }
                        }

                        if (!isTracking) {
                            isTracking = true;
                            set_mouse_pointer(enPointer);
                            handle_mouse_hover(true, pt);
                            TrackMouseEvent(&tme);
                        }

                        size_t vKey = wParam;
                        size_t state = decode_mouse_state(vKey);
                        handle_mouse_move(pt, state);
                        return 0;
                    }
                case WM_KEYDOWN:
                    {
                        size_t vKey = wParam;
                        size_t keycode = decode_keycode(vKey);
                        handle_key(UIE_KEY_DOWN, keycode);
                        break;
                    }
                case WM_KEYUP:
                    {
                        size_t vKey = wParam;
                        size_t keycode = decode_keycode(vKey);
                        handle_key(UIE_KEY_UP, keycode);
                        break;
                    }
                case WM_CHAR:
                    {
                        size_t charCode = wParam;
                        WORD keyFlags = HIWORD(lParam);
                        BOOL upFlag = (keyFlags & KF_UP) == KF_UP;   
                        ui_event_type_t type = upFlag ? UIE_KEY_UP : UIE_KEY_DOWN;
                        handle_char(type, charCode);
                        break;
                    }
                case WM_SETFOCUS:
                    {
                        handle_wm_focus(true);
                        return 0;
                    }
                case WM_KILLFOCUS:
                    {
                        handle_wm_focus(false);
                        return 0;
                    }  
                }
                
                return -1;
            }

            BOOL CALLBACK Win32Window::clipChildWindowCallback (HWND hwnd, LPARAM context)
            {
                    ClipInfo* clipInfo = (ClipInfo*) context;
                    RECT rect;
                    GetWindowRect(hwnd, &rect); 
                    HWND parent = GetParent(hwnd);
                    MapWindowPoints(HWND_DESKTOP, parent, (LPPOINT) &rect, 2);
                    int res = ExcludeClipRect (clipInfo->dc, rect.left, rect.top, rect.right, rect.bottom);
                    if (res == ERROR) {
                        lsp_debug("clipChildWindowCallback error %d", res);
                    }

                    return TRUE;
            }

            status_t Win32Window::init()
            {
                windowVisible = false;
                rendering = false;
                static_cast<Win32Display *>(pDisplay)->add_window(this);
                if (bWrapper) {
                    return STATUS_OK;
                }

                DWORD dwStyle = WS_CHILD;
                DWORD dwExStyle = 0;

                HWND ownerWnd = NULL;

                if (hwndNative != NULL) {
                    dwStyle = WS_CHILD;
                    ownerWnd = hwndNative;
                } else if (hwndParent != NULL) {
                    dwStyle = WS_CHILD | WS_CLIPSIBLINGS;
                    ownerWnd = hwndParent;
                    sSize.nWidth            = 1;
                    sSize.nHeight           = 1;
                } else {
                    dwStyle = WS_OVERLAPPEDWINDOW;
                    ownerWnd = hwndOwner;
                }

                // Calculate window constraints
                calc_constraints(&sSize, &sSize);

                WCHAR wndName[] = L"LSP Plugin";

                // Create the window.
                hwnd = CreateWindowExW(
                    dwExStyle,          // Optional window styles.
                    wndClsName,         // Window class
                    wndName,       // Window text
                    dwStyle,            // Window style

                    // Size and position
                    0, 0, sSize.nWidth, sSize.nHeight,

                    ownerWnd,    // Parent window    
                    NULL,        // Menu
                    hInstance,   // Instance handle
                    NULL         // Additional application data
                    );

                if (hwnd == NULL) {
                    DWORD errorwnd = GetLastError();
                } else {
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) this);
                    pSurface = new Win32CairoSurface(static_cast<Win32Display *>(pDisplay), hwnd, sSize.nWidth, sSize.nHeight);
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_LEAVE;
                    tme.dwHoverTime = HOVER_DEFAULT;
                    tme.hwndTrack = hwnd;
                    // Enable drag drop
                    RegisterDragDrop(hwnd, this);
                }
                set_mouse_pointer(MP_DEFAULT);

                return STATUS_OK;
            }

            void Win32Window::do_destroy() {
                //lsp_debug("DO_DESTROY");
                // if (nFlags & F_GRABBING)
                // {
                //     static_cast<Win32Display *>(pDisplay)->ungrab_events(this);
                //     nFlags &= ~F_GRABBING;
                // }
                set_handler(NULL);
                if (hwnd != NULL) {
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) 0);
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);
                    RevokeDragDrop(hwnd);
                }
                hdc = NULL;
                hwnd = NULL;
                hwndNative = NULL;
                hwndParent = NULL;
                pDisplay = NULL;
            }

            void Win32Window::destroy()
            {
                // Drop surface
                if (hwnd != NULL) {
                    SetLastError(0);
                    int res = SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) 0);
                    if (res == 0) {
                        DWORD err = GetLastError();
                        lsp_debug("SetWindowLongPtrW error %ld", err);
                    }
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);
                }
                hide();
                drop_surface();
                
                if (hwnd != NULL) {
                    if (DestroyWindow(hwnd) == 0) {
                        DWORD res = GetLastError();
                        lsp_debug("DestroyWindow error %ld", res);
                    }
                    GdiFlush();
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
                return IWindow::set_visibility(visible);
            }

            size_t Win32Window::screen()
            {
                return nScreen;
            }

            status_t Win32Window::set_caption(const char *ascii, const char *utf8)
            {
                if (hwnd != NULL) 
                {
                    LSPString caption;
                    caption.set_utf8(utf8);
                    SetWindowTextW(hwnd, caption.get_native_utf16());
                }
                
                return STATUS_OK;
            }

            void *Win32Window::handle()
            {
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

            void Win32Window::handle_mouse_move(POINT pt, size_t state) {
                event_t ue;
                ue.nType        = UIE_MOUSE_MOVE;
                ue.nLeft        = pt.x;
                ue.nTop         = pt.y;
                ue.nState       = state;
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                ue.nTime        = xts;

                handle_event(&ue);
            }

            void Win32Window::handle_mouse_hover(bool enter, POINT pt) {
                event_t ue;
                init_event(&ue);
                ue.nType        = enter ? UIE_MOUSE_IN : UIE_MOUSE_OUT;
                ue.nLeft        = pt.x;
                ue.nTop         = pt.y;
                handle_event(&ue);
            }

            void Win32Window::handle_mouse_button(code_t button, size_t type, POINT pt, size_t state) {

                event_t ue;
                init_event(&ue);
                ue.nCode        = button;
                ue.nType        = type;
                ue.nLeft        = pt.x;
                ue.nTop         = pt.y;
                size_t exState  = 1 << button;
                ue.nState       = state | exState;
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                ue.nTime        = xts;

                // Additionally generated event
                event_t gen;
                gen = ue;
                gen.nType       = UIE_UNKNOWN;

                HWND lockedWindow = GetCapture();
                if (type == UIE_MOUSE_DOWN) {
                    set_focus(true);
                    if (hwnd != lockedWindow) {
                        SetCapture(hwnd);
                    }
                    // Shift the buffer and push event
                    vBtnEvent[0]            = vBtnEvent[1];
                    vBtnEvent[1]            = vBtnEvent[2];
                    vBtnEvent[2].sDown      = ue;
                    init_event(&vBtnEvent[2].sUp);
                } else {
                    if (hwnd == lockedWindow) {
                        ReleaseCapture();
                    }
                    // Push event
                    vBtnEvent[2].sUp        = ue;
                    if (check_click(&vBtnEvent[2]))
                    {
                        gen.nType               = UIE_MOUSE_CLICK;
                        if (check_double_click(&vBtnEvent[1], &vBtnEvent[2]))
                        {
                            gen.nType               = UIE_MOUSE_DBL_CLICK;
                            if (check_double_click(&vBtnEvent[0], &vBtnEvent[1]))
                                gen.nType             = UIE_MOUSE_TRI_CLICK;
                        }
                    }
                }  

                IEventHandler *handler = pHandler;
                Win32Display* dspl = static_cast<Win32Display *>(pDisplay);
                Win32Window* src = this;
                bool isRoot = hwndParent == NULL;

                if (isRoot) {
                    dspl->prepare_dispatch(src);
                }

                if (handler != NULL) {
                    handler->handle_event(&ue);
                    // warning: *this Win32Window can be null at this time
                    // don't call member functions
                    if (gen.nType != UIE_UNKNOWN) {
                        handler->handle_event(&gen);
                    }
                }

                if (isRoot) {
                    dspl->dispatch_event(src, &ue);
                    if (gen.nType != UIE_UNKNOWN) {
                        dspl->dispatch_event(src, &gen);
                    }
                }
            }

            void Win32Window::handle_mouse_scroll(int delta, POINT pt, size_t state) {
                event_t ue;
                init_event(&ue);
                ue.nType        = UIE_MOUSE_SCROLL;
                if (delta > 0) {
                    ue.nCode        = MCD_UP;
                    ue.nState       = state | MCF_BUTTON4;
                } else if (delta < 0) {
                    ue.nCode        = MCD_DOWN;
                    ue.nState       = state | MCF_BUTTON5;
                } else {
                    return;
                }
                ue.nLeft        = pt.x;
                ue.nTop         = pt.y;
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                ue.nTime        = xts;

                handle_event(&ue);
            }

            void Win32Window::handle_key(ui_event_type_t type, size_t vKey) 
            {
                if (WSK_UNKNOWN == vKey)
                    return;

                POINT p;
                GetCursorPos(&p);
                MapWindowPoints(HWND_DESKTOP, hwnd, &p, 1);

                size_t vState = get_key_state();

                event_t ue;
                init_event(&ue);
                ue.nType = type;
                ue.nCode = vKey;
                ue.nState = vState;
                ue.nLeft = p.x;
                ue.nTop = p.y;
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                ue.nTime        = xts;

                handle_event(&ue);
            }

            void Win32Window::handle_char(ui_event_type_t type, size_t charCode) 
            {
                size_t vState = get_key_state();

                if ((vState & MCF_CONTROL) == MCF_CONTROL)
                    return; // Already handled
                switch (charCode)
                {
                case 0x08: // backspace
                case 0x0A: // linefeed 
                case 0x1B: // escape
                case 0x09: // tab
                case 0x0D: // carriage return
                    return;
                default:
                    break;
                }

                POINT p;
                GetCursorPos(&p);
                MapWindowPoints(HWND_DESKTOP, hwnd, &p, 1);

                event_t ue;
                init_event(&ue);
                ue.nType = type;
                ue.nCode = charCode;
                ue.nState = vState;
                ue.nLeft = p.x;
                ue.nTop = p.y;
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                ue.nTime        = xts;

                handle_event(&ue);
            }

            void Win32Window::handle_wm_paint() {
                if (pSurface != NULL && windowVisible) 
                {
                    rendering = true;
                    PAINTSTRUCT paintStruct;
                    HDC dc = BeginPaint(hwnd, &paintStruct);
                    
                    SetMapMode(dc, MM_TEXT);

                    // Clip child windows rectangles
                    ClipInfo clipInfo = { dc, this };
                    EnumChildWindows(hwnd, clipChildWindowCallback, (LPARAM) &clipInfo);

                    // Render on screen
                    static_cast<Win32CairoSurface *>(pSurface)->renderOffscreen(dc, sSize.nWidth, sSize.nHeight);
                    EndPaint(hwnd, &paintStruct);
                    
                    rendering = false;
                }
            }

            void Win32Window::handle_wm_size(UINT left, UINT right, UINT width, UINT height)
            {
                event_t ue;
                init_event(&ue);
                ue.nType        = UIE_RESIZE;
                ue.nLeft        = left;
                ue.nTop         = right;
                ue.nWidth       = width;
                ue.nHeight      = height;

                sSize.nLeft = left;
                sSize.nTop = right;
                sSize.nWidth = width;
                sSize.nHeight = height;
                
                if (!bWrapper) { 
                    if (pSurface != NULL) {
                        static_cast<Win32CairoSurface *>(pSurface)->resize(sSize.nWidth, sSize.nHeight);
                    }
                }
                handle_event(&ue);
            }

            void Win32Window::handle_wm_hide() {
                event_t ue;
                init_event(&ue);
                ue.nType        = UIE_HIDE;

                windowVisible = false;
                if (nFlags & F_GRABBING)
                {
                    static_cast<Win32Display *>(pDisplay)->ungrab_events(this);
                    nFlags &= ~F_GRABBING;
                }
                drop_surface();

                handle_event(&ue);
            }

            void Win32Window::handle_wm_show()
            {
                event_t ue;
                init_event(&ue);
                ue.nType        = UIE_SHOW;

                windowVisible = true;
                
                if (pSurface != NULL) {
                    static_cast<Win32CairoSurface *>(pSurface)->resize(sSize.nWidth, sSize.nHeight);
                }
                handle_event(&ue);
            }

            void Win32Window::handle_wm_focus(bool focused) {
                event_t ue;
                init_event(&ue);
                ue.nType        = focused ? UIE_FOCUS_IN : UIE_FOCUS_OUT;
                handle_event(&ue);
            }

            HRESULT Win32Window::DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
                if (pdwEffect == NULL)
                    return E_INVALIDARG;
                    
                FORMATETC format;
                format.cfFormat = CF_HDROP;
                format.ptd = NULL;
                format.dwAspect = DVASPECT_CONTENT;
                format.lindex = -1;
                format.tymed = TYMED_HGLOBAL;
                HRESULT res = pDataObj->QueryGetData(&format);
                if (SUCCEEDED(res)) {
                    POINT p;
                    p.x = pt.x;
                    p.y = pt.y;
                    MapWindowPoints(HWND_DESKTOP, hwnd, &p, 1);
                    event_t ue;
                    ue.nType            = UIE_DRAG_REQUEST;
                    ue.nLeft            = p.x;
                    ue.nTop             = p.y;
                    ue.nWidth           = 0;
                    ue.nHeight          = 0;
                    ue.nCode            = 0;
                    ue.nState           = DRAG_COPY;
                    system::time_t ts;
                    system::get_time(&ts);
                    timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                    ue.nTime        = xts;
                    handle_event(&ue);

                    RECT dragRect;
                    CopyRect(&dragRect, &static_cast<Win32Display *>(pDisplay)->dragRect);
                    if(PtInRect(&dragRect, p)) {
                        *pdwEffect = DROPEFFECT_COPY;
                    }else {
                        *pdwEffect = DROPEFFECT_NONE;
                    }
                } else {
                    *pdwEffect = DROPEFFECT_NONE;
                }
                return S_OK;
            }

            HRESULT Win32Window::DragLeave() {
                return S_OK;
            }

            HRESULT Win32Window::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
                if (pdwEffect == NULL)
                    return E_INVALIDARG;
                    
                POINT p;
                p.x = pt.x;
                p.y = pt.y;
                MapWindowPoints(HWND_DESKTOP, hwnd, &p, 1);
                event_t ue;
                ue.nType            = UIE_DRAG_REQUEST;
                ue.nLeft            = p.x;
                ue.nTop             = p.y;
                ue.nWidth           = 0;
                ue.nHeight          = 0;
                ue.nCode            = 0;
                ue.nState           = DRAG_COPY;
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                ue.nTime        = xts;
                handle_event(&ue);

                RECT dragRect;
                CopyRect(&dragRect, &static_cast<Win32Display *>(pDisplay)->dragRect);
                if(PtInRect(&dragRect, p)) {
                    *pdwEffect = DROPEFFECT_COPY;
                }else {
                    *pdwEffect = DROPEFFECT_NONE;
                }
                return S_OK;
            }

            HRESULT Win32Window::Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
                if (pdwEffect == NULL)
                    return E_INVALIDARG;

                FORMATETC format;
                format.cfFormat = CF_HDROP;
                format.ptd = NULL;
                format.dwAspect = DVASPECT_CONTENT;
                format.lindex = -1;
                format.tymed = TYMED_HGLOBAL;
                HRESULT resHDROP = pDataObj->QueryGetData(&format);
                if (SUCCEEDED(resHDROP)) {
                    uSTGMEDIUM medium;
                    medium.tymed = TYMED_HGLOBAL;
                    medium.pUnkForRelease = NULL;
                    HRESULT dres = pDataObj->GetData(&format, &medium);
                    if (SUCCEEDED(dres)) {
                        HDROP hdrop = (HDROP)medium.hGlobal;
                        WCHAR filename[MAX_PATH];
                        int wnb = DragQueryFileW(hdrop, 0, filename, MAX_PATH);

                        if (wnb > 0)
                            static_cast<Win32Display *>(pDisplay)->setDroppedFile(filename);

                        GlobalFree(hdrop);
                        ReleaseStgMedium(&medium);
                        
                        POINT p;
                        p.x = pt.x;
                        p.y = pt.y;
                        MapWindowPoints(HWND_DESKTOP, hwnd, &p, 1);
                        event_t ue;
                        ue.nType            = UIE_DRAG_REQUEST;
                        ue.nLeft            = p.x;
                        ue.nTop             = p.y;
                        ue.nWidth           = 0;
                        ue.nHeight          = 0;
                        ue.nCode            = 0;
                        ue.nState           = DRAG_COPY;
                        system::time_t ts;
                        system::get_time(&ts);
                        timestamp_t xts     = (timestamp_t(ts.seconds) * 1000);
                        ue.nTime        = xts;
                        handle_event(&ue);
                    }
                }

                *pdwEffect = DROPEFFECT_NONE;
                return S_OK;
            }

            ULONG Win32Window::AddRef() {
                return 1;
            }

            ULONG Win32Window::Release() {
                return 0;
            }

            HRESULT Win32Window::QueryInterface(REFIID riid, void **ppvObject) {
                if (ppvObject == NULL)
                    return E_POINTER;
                if (IID_IDropTarget == riid) {
                    *ppvObject = dynamic_cast<IDropTarget*>(this);
                    return S_OK;
                }
                return E_NOINTERFACE;
            }

            status_t Win32Window::move(ssize_t left, ssize_t top)
            {
                //lsp_debug("move => left : %ld, top : %ld", left, top);
                sSize.nLeft    = left;
                sSize.nTop   = top;
                if (!bWrapper) {
                    setWndPos();
                }
                return STATUS_OK;
            }

            status_t Win32Window::resize(ssize_t width, ssize_t height)
            {
                //lsp_debug("resize => width : %ld, height : %ld", width, height);
                sSize.nWidth    = width;
                sSize.nHeight   = height;

                int screenWidth = GetSystemMetrics(SM_CXSCREEN); 
                rootWndPos.x = (screenWidth - sSize.nWidth ) / 2;
                int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                rootWndPos.y = (screenHeight - sSize.nHeight ) / 2;

                if (!bWrapper) {
                    setWndPos();
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
                setWndPos();
                return STATUS_OK;
            }

            status_t Win32Window::set_border_style(border_style_t style)
            {
                enBorderStyle = style;
                switch (style)
                {
                case BS_DIALOG:
                    {
                        if (hwnd != NULL && hwndParent == NULL && hwndNative == NULL) {
                            SetWindowLongPtrW(hwnd, GWL_STYLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX);
                            SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);
                        }
                    }
                    break;
                default:
                    break;
                }
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
                if (nFlags & F_GRABBING)
                {
                    static_cast<Win32Display *>(pDisplay)->ungrab_events(this);
                    nFlags &= ~F_GRABBING;
                }
                if (hwnd == NULL) {
                    return STATUS_BAD_STATE;
                }
                if (GetFocus() == hwnd) {
                    set_focus(false);
                }
                if (pSurface != NULL) {
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOZORDER | SWP_NOACTIVATE);
                }
                return STATUS_OK;
            }

            void Win32Window::setWndPos() {
                WINDOWINFO wndInfo;
                wndInfo.cbSize = sizeof(WINDOWINFO);
                GetWindowInfo(hwnd, &wndInfo);
                UINT extraWidth = (wndInfo.rcClient.left - wndInfo.rcWindow.left) + (wndInfo.rcWindow.right - wndInfo.rcClient.right);
                UINT extraHeight = (wndInfo.rcClient.top - wndInfo.rcWindow.top) + (wndInfo.rcWindow.bottom - wndInfo.rcClient.bottom);
                UINT leftBorderOffset = 0;
                UINT topBorderOffset = 0;
                if (hwndParent != NULL) {
                    leftBorderOffset = -CHILD_BORDER_WIDTH;
                    topBorderOffset = -CHILD_BORDER_WIDTH;
                    extraWidth = CHILD_BORDER_WIDTH * 2;
                    extraHeight = CHILD_BORDER_WIDTH * 2;
                }
                if (hwndNative == NULL && hwndParent == NULL) {
                    SetWindowPos(hwnd, NULL, rootWndPos.x, rootWndPos.y, sSize.nWidth + extraWidth,sSize.nHeight + extraHeight, SWP_NOCOPYBITS | SWP_NOZORDER | SWP_NOACTIVATE);
                } else {
                    SetWindowPos(hwnd, NULL,sSize.nLeft + leftBorderOffset, sSize.nTop + topBorderOffset,sSize.nWidth + extraWidth,sSize.nHeight + extraHeight, SWP_NOCOPYBITS | SWP_NOZORDER | SWP_NOACTIVATE);
                }
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
                    return STATUS_BAD_STATE;
                }
                if (pSurface == NULL) {
                    pSurface = new Win32CairoSurface(static_cast<Win32Display *>(pDisplay), hwnd, sSize.nWidth, sSize.nHeight);
                }
                ShowWindow(this->hwnd, SW_SHOWNORMAL);
                SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                if (hwndOwner != NULL) {
                    SetForegroundWindow(hwnd);
                }
                return STATUS_OK;
            }

            status_t Win32Window::set_left(ssize_t left)
            {
                int screenWidth = GetSystemMetrics(SM_CXSCREEN); 
                rootWndPos.x = (screenWidth - sSize.nWidth ) / 2;
                return move(left, sSize.nTop);
            }

            status_t Win32Window::set_top(ssize_t top)
            {
                int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                rootWndPos.y = (screenHeight - sSize.nHeight ) / 2;
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
                
                RECT rect;
                GetClientRect(hwnd, &rect);
                if (sSize.nWidth != (rect.right - rect.left) || sSize.nHeight != (rect.bottom - rect.top)) {
                    setWndPos();
                }
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
                setWndPos();
                return STATUS_OK;
            }

            status_t Win32Window::set_focus(bool focus)
            {
                if (hwnd != NULL && focus) {
                    if (hwndNative != NULL) {
                        SetFocus(hwndNative);
                    } else {
                        SetFocus(hwnd);
                    }
                } else {
                    Win32Display* dpl = static_cast<Win32Display *>(pDisplay);
                    if (dpl != NULL) {
                        Win32Window* root = dpl->findRootWindow(dpl->default_screen());
                        if (root != NULL && root->hwndNative != NULL) {
                            SetFocus(root->hwndNative);
                        } else if (root != NULL && root->hwnd != NULL) {
                            SetFocus(root->hwnd);
                        }
                    }
                }
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
                nActions            = actions;
                return STATUS_OK;
            }

            status_t Win32Window::set_mouse_pointer(mouse_pointer_t pointer)
            {
                if (hwnd == NULL)
                    return STATUS_BAD_STATE;

                HANDLE cur = static_cast<Win32Display *>(pDisplay)->get_cursor(pointer);
                if (cur == NULL)
                    return STATUS_UNKNOWN_ERR;

                SetCursor((HCURSOR)cur);
                
                enPointer = pointer;
                return STATUS_OK;
            }

            mouse_pointer_t Win32Window::get_mouse_pointer()
            {
                return enPointer;
            }

            status_t Win32Window::grab_events(grab_t group)
            {
                if (!(nFlags & F_GRABBING))
                {
                    static_cast<Win32Display *>(pDisplay)->grab_events(this, group);
                    nFlags |= F_GRABBING;
                }
                return STATUS_OK;
            }

            status_t Win32Window::ungrab_events()
            {
                if (!(nFlags & F_GRABBING))
                    return STATUS_NO_GRAB;
                return static_cast<Win32Display *>(pDisplay)->ungrab_events(this);
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