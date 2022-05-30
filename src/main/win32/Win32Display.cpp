#include <lsp-plug.in/common/types.h>

#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/stdlib/stdio.h>
#include <lsp-plug.in/ws/types.h>
#include <lsp-plug.in/ws/keycodes.h>
#include <lsp-plug.in/io/OutMemoryStream.h>
#include <lsp-plug.in/io/InFileStream.h>
#include <lsp-plug.in/runtime/system.h>

#include <private/win32/Win32Display.h>
#include <private/win32/Win32CairoSurface.h>

// End mark for main PeekMessage loop
#define        USER_LOOP_END     (WM_USER + 0)

// OEM cursors (from winuser.h)
#define OCR_NORMAL 32512
#define OCR_IBEAM 32513
#define OCR_WAIT 32514
#define OCR_CROSS 32515
#define OCR_UP 32516
#define OCR_SIZE 32640
#define OCR_ICON 32641
#define OCR_SIZENWSE 32642
#define OCR_SIZENESW 32643
#define OCR_SIZEWE 32644
#define OCR_SIZENS 32645
#define OCR_SIZEALL 32646
#define OCR_ICOCUR 32647
#define OCR_NO 32648
#define OCR_HAND 32649
#define OCR_APPSTARTING 32650

static HMODULE currentModuleHandle = NULL;
static HMODULE currentDllModuleHandle = NULL;

namespace lsp
{
    namespace ws
    {
        namespace win32
        {
            static int cursor_shapes[] =
            {
                -1,                         // MP_NONE
                OCR_NORMAL,                  // MP_ARROW
                OCR_SIZEALL,                // MP_ARROW_LEFT
                OCR_SIZEALL,                // MP_ARROW_RIGHT
                OCR_SIZEALL,                // MP_ARROW_UP
                OCR_SIZEALL,                // MP_ARROW_DOWN
                OCR_HAND,                   // MP_HAND
                OCR_CROSS,                  // MP_CROSS
                OCR_IBEAM,                  // MP_IBEAM
                OCR_HAND,                   // MP_DRAW
                OCR_CROSS,                   // MP_PLUS
                OCR_SIZENESW,               // MP_SIZE_NESW
                OCR_SIZENS,                 // MP_SIZE_NS
                OCR_SIZEWE,                 // MP_SIZE_WE
                OCR_SIZENWSE,               // MP_SIZE_NWSE
                OCR_UP,                // MP_UP_ARROW
                OCR_WAIT,                   // MP_HOURGLASS
                OCR_HAND,                   // MP_DRAG
                OCR_NO,                     // MP_NO_DROP
                OCR_NO,                     // MP_DANGER
                OCR_SIZEWE,                 // MP_HSPLIT
                OCR_SIZENS,                 // MP_VPSLIT
                OCR_HAND,                   // MP_MULTIDRAG
                OCR_APPSTARTING,            // MP_APP_START
                OCR_NORMAL                    // MP_HELP
            };

            Win32Display::Win32Display()
            {
                bExit           = false;
                hFtLibrary = NULL;
                bzero(&sCairoUserDataKey, sizeof(sCairoUserDataKey));
                wndClassName = std::wstring(L"LSP_Window");
            }

            Win32Display::~Win32Display()
            {
            }

            LRESULT CALLBACK Win32Display::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
            {
                Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
                if (window) return window->internalWindowProc(hwnd, msg, wParam, lParam);
               
                return DefWindowProcW(hwnd, msg, wParam, lParam);
            }

            const HMODULE Win32Display::GetCurrentModule()
            {
                if (currentModuleHandle == NULL)
                {
                    static int lpMod = 0;
                    BOOL status = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                    (LPCWSTR) this,
                                                    &currentModuleHandle);
                }
                return currentModuleHandle;
            }

            const HMODULE Win32Display::GetCurrentDllModule()
            {
                if (currentDllModuleHandle == NULL)
                {
                    static int lpMod = 0;
                    BOOL status = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                    (LPCWSTR) &lpMod,
                                                    &currentDllModuleHandle);
                }
                return currentDllModuleHandle;
            }

            status_t Win32Display::init(int argc, const char **argv)
            {
                // Get Main application icon
                WCHAR szPath[MAX_PATH];
                WCHAR filename[] = L"lsp.ico";
                //WORD iconIdx;
                GetModuleFileNameW(GetCurrentDllModule(), szPath, MAX_PATH);

                LSPString str;
                str.append_utf16(szPath);
                io::Path path;
                path.set(&str);
                path.parent();
                path.append_child("lsp.ico");
                WCHAR* szDirPath = path.as_string()->clone_utf16();

                //mainIcon = ExtractAssociatedIconW(GetCurrentModule(), szPath, &iconIdx);
                mainIcon = (HICON)LoadImageW(NULL, szDirPath, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
                // Register the window class.
                hInstance = GetCurrentModule();
                WNDCLASSEXW wc = { };
                wc.cbSize = sizeof(WNDCLASSEXW);
                wc.lpfnWndProc   = WndProc;
                wc.hInstance     = hInstance;
                wc.lpszClassName = wndClassName.c_str();
                wc.style = 0;
                wc.hIcon = mainIcon;
                wc.hbrBackground = NULL;

                ATOM registerRes = RegisterClassExW(&wc);
                if (registerRes == 0) {
                    DWORD errorReg = GetLastError();
                    lsp_debug("RegisterClassExW LAST ERROR %ld", errorReg);
                }

                // Initialize cursors
                for (size_t i=0; i<__MP_COUNT; ++i)
                {
                    vCursors[i] = LoadImageW(NULL, MAKEINTRESOURCEW(cursor_shapes[i]), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
                }
                return STATUS_OK;
                //return IDisplay::init(argc, argv);
            }

            HANDLE Win32Display::get_cursor(mouse_pointer_t pointer)
            {
                if (pointer == MP_DEFAULT)
                    pointer = MP_ARROW;
                else if ((pointer < 0) || (pointer > __MP_COUNT))
                    pointer = MP_NONE;

                return vCursors[pointer];
            }

            IWindow *Win32Display::create_window()
            {
                int nScreen = findFreeScreenNumber();
                HWND ownerHwnd = NULL;
                Win32Window* sOwnerWnd = findRootWindow(default_screen());
                if (sOwnerWnd != NULL) {
                    ownerHwnd = sOwnerWnd->hwnd;
                }
                Win32Window* root = new Win32Window(this, NULL, NULL, NULL, ownerHwnd, nScreen, false, hInstance, wndClassName.c_str());
                rootWindows.add(root);
                return root;
            }

            IWindow *Win32Display::create_window(size_t screen)
            {
                Win32Window* sRootWnd = findRootWindow(screen);
                if (sRootWnd != NULL) {
                    return new Win32Window(this, NULL, NULL, sRootWnd->hwnd, NULL, screen, false, hInstance, wndClassName.c_str());
                }
                return NULL;
            }

            IWindow *Win32Display::create_window(void *handle)
            {
                lsp_trace("handle = %p", handle);

                int nScreen = findFreeScreenNumber();
                Win32Window* root = new Win32Window(this, NULL, handle, NULL, NULL, nScreen, false, hInstance, wndClassName.c_str());
                rootWindows.add(root);
                return root;
            }

            IWindow *Win32Display::wrap_window(void *handle)
            {
                int nScreen = findFreeScreenNumber();
                Win32Window* root = new Win32Window(this, NULL, handle, NULL, NULL, nScreen, true, hInstance, wndClassName.c_str());
                rootWindows.add(root);
                return root;
            }

            Win32Window* Win32Display::findRootWindow(int screen) {
                Win32Window* sRootWnd = NULL;
                for (int i=0, nroot = rootWindows.size(); i<nroot; ++i) {
                    if (rootWindows.get(i)->screen() == screen) {
                        sRootWnd = rootWindows.get(i);
                        break;
                    }
                }
                return sRootWnd;
            }

            int Win32Display::findScreenOwner(HWND hwnd) {
                int screenOwner = 0;
                for (int i=0, nroot = rootWindows.size(); i<nroot; ++i) {
                    if (rootWindows.get(i)->hwnd == hwnd) {
                        screenOwner = rootWindows.get(i)->screen();
                        break;
                    }
                }
                return screenOwner;
            }
            
            int Win32Display::findFreeScreenNumber(){
                int nScreen = 1;
                bool found = true;
                do {
                    found = true;
                    for (int i=0, nroot = rootWindows.size(); i<nroot; ++i) {
                        if (rootWindows.get(i)->screen() == nScreen) {
                            nScreen++;
                            found = false;
                            break;
                        }
                    }
                } while (!found);
                return nScreen;
            }

            ISurface *Win32Display::create_surface(size_t width, size_t height)
            {
                return new Win32CairoSurface(this, width, height);
            }

            void Win32Display::destroy()
            {
                rootWindows.flush();
                vWindows.flush();
                sPending.flush();
                for (size_t i=0; i<__GRAB_TOTAL; ++i)
                    vGrab[i].clear();
                sTargets.clear();

                DestroyIcon(mainIcon);

                IDisplay::destroy();
            }

            status_t Win32Display::main()
            {
                system::time_t ts;

                while (!bExit)
                {
                    // Get current time
                    system::get_time(&ts);
                    timestamp_t xts     = (timestamp_t(ts.seconds) * 1000) + (ts.nanos / 1000000);
                    int wtime           = 50; // How many milliseconds to wait

                    if (sTasks.size() > 0)
                    {
                        dtask_t *t          = sTasks.first();
                        ssize_t delta       = t->nTime - xts;
                        if (delta <= 0)
                            wtime               = -1;
                        else if (delta <= wtime)
                            wtime               = delta;
                    }
                    // else if (::XPending(pDisplay) > 0)
                    //     wtime               = 0;
                    
                    if (wtime <= 0)
                    {
                        // Do iteration
                        status_t result = IDisplay::main_iteration();
                        if (result == STATUS_OK)
                            result = do_main_iteration(xts);
                        if (result != STATUS_OK)
                            return result;
                    }
                }

                return STATUS_OK;
            }

            status_t Win32Display::wait_events(wssize_t millis)
            {
                if (bExit)
                    return STATUS_OK;

                system::time_t ts;
                // Get current time
                system::get_time(&ts);

                timestamp_t xts         = (timestamp_t(ts.seconds) * 1000) + (ts.nanos / 1000000);
                timestamp_t deadline    = xts + millis;

                MSG msg;
                system::time_t wmTs;

                do
                {
                    wssize_t wtime      = deadline - xts; // How many milliseconds to wait

                    if (sTasks.size() > 0)
                    {
                        dtask_t *t          = sTasks.first();
                        ssize_t delta       = t->nTime - xts;
                        if (delta <= 0)
                            wtime               = -1;
                        else if (delta <= wtime)
                            wtime               = delta;
                    }
                    else if (GetQueueStatus(QS_ALLEVENTS) != 0)
                        wtime               = 0;

                    system::get_time(&wmTs);
                    timestamp_t wmTsMsBegin     = (timestamp_t(wmTs.seconds) * 1000) + (wmTs.nanos / 1000000);
                    timestamp_t wmTsMsEnd     = wmTsMsBegin + wtime; // How many milliseconds to wait
                    timestamp_t counter       = wmTsMsBegin;

                    int queueStatus = 0;

                    while (counter < wmTsMsEnd) 
                    { 
                        if (GetQueueStatus(QS_ALLEVENTS) == 0) {
                            system::get_time(&wmTs);
                            counter     = (timestamp_t(wmTs.seconds) * 1000) + (wmTs.nanos / 1000000);
                        } else {
                            queueStatus = 1;
                            break;
                        }
                    }

                    if ((wtime <= 0) || (queueStatus > 0))
                        break;

                    // Get current time
                    system::get_time(&ts);
                    xts         = (timestamp_t(ts.seconds) * 1000) + (ts.nanos / 1000000);
                } while (!bExit);

                return STATUS_OK;
            }

            bool Win32Display::add_window(Win32Window *wnd)
            {
                return vWindows.add(wnd);
            }

            bool Win32Display::remove_window(Win32Window *wnd)
            {
                // Remove focus window
                // if (pFocusWindow == wnd)
                //     pFocusWindow = NULL;

                // Remove window from list
                rootWindows.premove(wnd);
                if (!vWindows.premove(wnd))
                    return false;

                // Check if need to leave main cycle
                if (vWindows.size() <= 0) {
                    bExit = true;
                }
                    
                return true;
            }

            status_t Win32Display::grab_events(Win32Window *wnd, grab_t group)
            {
                // Check validity of argument
                if (group >= __GRAB_TOTAL)
                    return STATUS_BAD_ARGUMENTS;

                // Check that window does not belong to any active grab group
                for (int i=0; i<__GRAB_TOTAL; ++i)
                {
                    lltl::parray<Win32Window> &g = vGrab[i];
                    if (g.index_of(wnd) >= 0)
                    {
                        lsp_warn("Grab duplicated for window %p", wnd);
                        return STATUS_DUPLICATED;
                    }
                }

                lltl::parray<Win32Window> &g = vGrab[group];
                // Add a grab
                if (g.add(wnd))
                    return STATUS_NO_MEM;

                return STATUS_OK;
            }

            status_t Win32Display::ungrab_events(Win32Window *wnd)
            {
                bool found = false;
                // Check that window does belong to any active grab group
                for (int i=0; i<__GRAB_TOTAL; ++i)
                {
                    lltl::parray<Win32Window> &g = vGrab[i];
                    if (g.premove(wnd))
                    {
                        found = true;
                        break;
                    }
                }

                // Return error if not found
                if (!found)
                {
                    return STATUS_NO_GRAB;
                }

                return STATUS_OK;
            }

            bool Win32Display::prepare_dispatch(Win32Window* src) {
                // Clear the collection
                sTargets.clear();
                // Check if there is grab enabled and obtain list of receivers
                bool has_grab = false;
                for (int i=__GRAB_TOTAL-1; i>=0; --i)
                {
                    lltl::parray<Win32Window> &g = vGrab[i];
                    if (g.size() > 0) {
                        // Add listeners from grabbing windows
                        for (int j=0; j<g.size(); ++j)
                        {
                            Win32Window *wnd = g.uget(j);
                            if ((wnd == NULL) || (vWindows.index_of(wnd) < 0)) {
                                g.remove(j);
                            } 
                            else if (wnd != src) {
                                sTargets.add(wnd);
                            }
                        }
                    }

                    // Finally, break if there are target windows
                    if (sTargets.size() > 0)
                    {
                        has_grab = true;
                        break;
                    }
                }
                return has_grab;
            }

            bool Win32Display::dispatch_event(Win32Window* src, event_t *ev) 
            {
                bool has_grab = false;
                if (sTargets.size() > 0)
                {
                    has_grab = true;
                }

                event_t ud;
                ud = *ev;
                ud.nState       = 0;

                // Deliver the message to target windows
                for (size_t i=0, nwnd = sTargets.size(); i<nwnd; ++i)
                {
                    Win32Window *wnd = sTargets.uget(i);

                    if ((wnd == NULL) || (vWindows.index_of(wnd) < 0)) {
                        sTargets.remove(i);
                    } else {
                        RECT rect;
                        MapWindowPoints(src->hwnd, wnd->hwnd, (LPPOINT) &rect, 2);
                        ud.nLeft = rect.left;
                        ud.nTop = rect.top;
                        wnd->handle_event(&ud);
                    }
                }
                return has_grab;
            }

            void Win32Display::sync()
            { 
                GdiFlush();
            }

            status_t Win32Display::main_iteration()
            {
                // Get current time to determine if need perform a rendering
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts = (timestamp_t(ts.seconds) * 1000) + (ts.nanos / 1000000);

                // Do iteration
                return do_main_iteration(xts);
            }

            status_t Win32Display::do_main_iteration(timestamp_t ts)
            {
                status_t result = STATUS_OK;
                MSG msg; 

                Win32Window* rtWd = findRootWindow(default_screen());
                HWND nativeHwnd = NULL;
                if (rtWd != NULL) {
                    nativeHwnd = rtWd->hwndNative;
                }
                
                if (nativeHwnd == NULL) {
                    long loopTime = 0;
                    PostMessageW(NULL, USER_LOOP_END, 0, 0);
                    while (PeekMessageW(&msg, NULL,  0, 0, PM_REMOVE) != 0)
                    { 
                         if (msg.message == USER_LOOP_END) {
                            loopTime = GetMessageTime();
                        } else {
                            TranslateMessage(&msg);
                            DispatchMessageW(&msg); 
                            if (loopTime != 0 && GetMessageTime() > loopTime) {
                                break;
                            }
                        }
                    }
                }

                // Generate list of tasks for processing
                sPending.clear();

                while (true)
                {
                    // Get next task
                    dtask_t *t  = sTasks.first();
                    if (t == NULL)
                        break;

                    // Do we need to process this task ?
                    if (t->nTime > ts)
                        break;

                    // Allocate task in pending queue
                    t   = sPending.append();
                    if (t == NULL)
                        return STATUS_NO_MEM;

                    // Remove the task from the queue
                    if (!sTasks.remove(0, t))
                    {
                        result = STATUS_UNKNOWN_ERR;
                        break;
                    }
                }

                // Process pending tasks
                if (result == STATUS_OK)
                {
                    // Execute all tasks in pending queue
                    for (size_t i=0; i<sPending.size(); ++i)
                    {
                        dtask_t *t  = sPending.uget(i);

                        ////lsp_debug("Process task : %ld", t->nID);
                        // Process task
                        result  = t->pHandler(t->nTime, ts, t->pArg);
                        if (result != STATUS_OK)
                            break;
                    }
                }

                if (nativeHwnd != NULL) {
                    for (int i=0, nwnd = rootWindows.size(); i<nwnd; ++i) {
                        Win32Window* rtw = rootWindows.get(i);
                        if (rtw != NULL && rtw->hwnd != NULL && PeekMessageW(&msg, rtw->hwnd,  WM_PAINT, WM_PAINT, PM_REMOVE) != 0) {
                            rtw->handle_wm_paint();
                        }
                    }
                }

                // Call for main task
                call_main_task(ts);

                // Return number of processed events
                return result;
            }

            void Win32Display::quit_main()
            {
                bExit = true;
            }

            size_t Win32Display::screens()
            {
                return rootWindows.size() + 1;
            }

            size_t Win32Display::default_screen()
            {
                return 1;
            }

            status_t Win32Display::screen_size(size_t screen, ssize_t *w, ssize_t *h)
            {
                Win32Window* sRootWnd = findRootWindow(screen);
                if (sRootWnd == NULL)
                    return STATUS_BAD_STATE;
                
                if (w != NULL)
                    *w = sRootWnd->width();
                if (h != NULL)
                    *h = sRootWnd->height();

                return STATUS_OK;
            }

            status_t Win32Display::set_clipboard(size_t id, IDataSource *ds)
            {
                return STATUS_OK;
            }

            status_t Win32Display::get_clipboard(size_t id, IDataSink *dst)
            {
                return STATUS_OK;
            }

            const char * const *Win32Display::get_drag_ctypes()
            {
                return NULL;
            }

            status_t Win32Display::reject_drag()
            {
                return STATUS_OK;
            }

            status_t Win32Display::accept_drag(IDataSink *sink, drag_t action, bool internal, const rectangle_t *r)
            {
                return STATUS_OK;
            }

            status_t Win32Display::get_pointer_location(size_t *screen, ssize_t *left, ssize_t *top)
            {
                POINT p;
                HWND hwndPointer;
                int screenOwner;
                // Get cursor position in screen coordinates
                if (GetCursorPos(&p)) {
                    // Get window handle below cursor
                    if ((hwndPointer = WindowFromPoint(p)) != NULL) {
                        // Get screen below cursor
                        if ((screenOwner = findScreenOwner(hwndPointer)) > 0) {
                            Win32Window* sRootWnd = findRootWindow(screenOwner);
                            // Map cursor position in window coordinates
                            if (sRootWnd != NULL && sRootWnd->hwnd != NULL && ScreenToClient(sRootWnd->hwnd, &p)) {
                                if (screen != NULL)
                                    *screen = screenOwner;
                                if (left != NULL)
                                    *left   = p.x;
                                if (top != NULL)
                                    *top    = p.y;

                                return STATUS_OK;
                            }
                        }
                    }
                }
                return STATUS_NOT_FOUND;
            }

            status_t Win32Display::add_font(const char* name, const char* path)
            {
                if ((name == NULL) || (path == NULL))
                    return STATUS_BAD_ARGUMENTS;

                LSPString tmp;
                if (!tmp.set_utf8(path))
                    return STATUS_NO_MEM;

                return add_font(name, &tmp);
            }

            status_t Win32Display::add_font(const char* name, const io::Path* path)
            {
                if ((name == NULL) || (path == NULL))
                    return STATUS_BAD_ARGUMENTS;
                return add_font(name, path->as_utf8());
            }

            status_t Win32Display::add_font(const char* name, const LSPString* path)
            {
                if (name == NULL)
                    return STATUS_BAD_ARGUMENTS;

                io::InFileStream ifs;
                status_t res = ifs.open(path);
                if (res != STATUS_OK)
                    return res;

                //lsp_debug("Loading font '%s' from file '%s'", name, path->get_native());
                res = add_font(name, &ifs);
                status_t res2 = ifs.close();

                return (res == STATUS_OK) ? res2 : res;
            }

            status_t Win32Display::add_font(const char *name, io::IInStream *is)
            {
                if ((name == NULL) || (is == NULL))
                    return STATUS_BAD_ARGUMENTS;

                if (vCustomFonts.exists(name))
                    return STATUS_ALREADY_EXISTS;

                status_t res = init_freetype_library();
                if (res != STATUS_OK)
                    return res;

                // Read the contents of the font
                io::OutMemoryStream os;
                wssize_t bytes = is->sink(&os);
                if (bytes < 0)
                    return -bytes;

                font_t *f = alloc_font_object(name);
                if (f == NULL)
                    return STATUS_NO_MEM;

                // Create new font face
                f->data = os.release();
                FT_Error ft_status = FT_New_Memory_Face(hFtLibrary, static_cast<const FT_Byte *>(f->data), bytes, 0, &f->ft_face);
                if (ft_status != 0)
                {
                    unload_font_object(f);
                    lsp_error("FT_MANAGE Error creating freetype font face for font '%s', error=%d", f->name, int(ft_status));
                    return STATUS_UNKNOWN_ERR;
                }

                // Register font data
                if (!vCustomFonts.create(name, f))
                {
                    unload_font_object(f);
                    return STATUS_NO_MEM;
                }

                lsp_trace("FT_MANAGE loaded font this=%p, font='%s', refs=%d, bytes=%ld", f, f->name, int(f->refs), long(bytes));

                return STATUS_OK;
            }

            status_t Win32Display::init_freetype_library()
            {
                if (hFtLibrary != NULL)
                    return STATUS_OK;

                // Initialize FreeType handle
                FT_Error status = FT_Init_FreeType (& hFtLibrary);
                if (status != 0)
                {
                    lsp_error("Error %d opening library.\n", int(status));
                    return STATUS_UNKNOWN_ERR;
                }

                return STATUS_OK;
            }

            Win32Display::font_t *Win32Display::alloc_font_object(const char *name)
            {
                font_t *f       = static_cast<font_t *>(malloc(sizeof(font_t)));
                if (f == NULL)
                    return NULL;

                // Copy font name
                if (!(f->name = strdup(name)))
                {
                    free(f);
                    return NULL;
                }

                f->alias        = NULL;
                f->data         = NULL;
                f->ft_face      = NULL;
                f->refs         = 1;

            //#ifdef USE_LIBCAIRO
                for (size_t i=0; i<4; ++i)
                    f->cr_face[i]   = NULL;
            //#endif /* USE_LIBCAIRO */

                return f;
            }

            void Win32Display::unload_font_object(font_t *f)
            {
                if (f == NULL)
                    return;

            // Fist call nested libraries to release the resource
            //#ifdef USE_LIBCAIRO
                for (size_t i=0; i<4; ++i)
                    if (f->cr_face[i] != NULL)
                    {
                        lsp_trace(
                            "FT_MANAGE call cairo_font_face_destroy[%d] references=%d",
                            int(i), int(cairo_font_face_get_reference_count(f->cr_face[i]))
                        );
                        cairo_font_face_destroy(f->cr_face[i]);
                        f->cr_face[i]   = NULL;
                    }
            //#endif /* USE_LIBCAIRO */

                // Destroy font object
                lsp_trace("FT_MANAGE call destroy_font_object");
                destroy_font_object(f);
            }

            void Win32Display::destroy_font_object(font_t *f)
            {
                if (f == NULL)
                    return;

                lsp_trace("FT_MANAGE this=%p, name='%s', refs=%d", f, f->name, int(f->refs));
                if ((--f->refs) > 0)
                    return;

                lsp_trace("FT_MANAGE   destroying font this=%p, name='%s'", f, f->name);

                // Destroy font face if present
                if (f->ft_face != NULL)
                {
                    FT_Done_Face(f->ft_face);
                    f->ft_face  = NULL;
                }

                // Deallocate font data
                if (f->data != NULL)
                {
                    free(f->data);
                    f->data     = NULL;
                }

                // Deallocate alias if present
                if (f->alias != NULL)
                {
                    free(f->alias);
                    f->alias    = NULL;
                }

                // Deallocate font object
                free(f);
            }

            Win32Display::font_t *Win32Display::get_font(const char *name)
            {
                lltl::pphash<char, font_t> path;

                while (true)
                {
                    // Fetch the font record
                    font_t *res = vCustomFonts.get(name);
                    if (res == NULL)
                        return NULL;
                    else if (res->ft_face != NULL)
                        return res;
                    else if (res->alias == NULL)
                        return NULL;

                    // Font alias, need to follow it
                    if (!path.create(name, res))
                        return NULL;
                    name        = res->alias;
                }
            }

            status_t Win32Display::add_font_alias(const char* name, const char* alias)
            {
                if ((name == NULL) || (alias == NULL))
                    return STATUS_BAD_ARGUMENTS;

                if (vCustomFonts.exists(name))
                    return STATUS_ALREADY_EXISTS;

                font_t *f = alloc_font_object(name);
                if (f == NULL)
                    return STATUS_NO_MEM;
                if ((f->alias = strdup(alias)) == NULL)
                {
                    unload_font_object(f);
                    return STATUS_NO_MEM;
                }

                // Register font data
                if (!vCustomFonts.create(name, f))
                {
                    unload_font_object(f);
                    return STATUS_NO_MEM;
                }

                return STATUS_OK;
            }

            status_t Win32Display::remove_font(const char* name)
            {
                if (name == NULL)
                    return STATUS_BAD_ARGUMENTS;

                font_t *f = NULL;
                if (!vCustomFonts.remove(name, &f))
                    return STATUS_NOT_FOUND;

                unload_font_object(f);
                return STATUS_OK;
            }

            void Win32Display::remove_all_fonts()
            {
                // Deallocate previously allocated fonts
                lsp_trace("FT_MANAGE removing all previously loaded fonts");
                drop_custom_fonts();
            }

            void Win32Display::drop_custom_fonts()
            {
                lltl::parray<font_t> fonts;
                vCustomFonts.values(&fonts);
                vCustomFonts.flush();

                // Destroy all font objects
                for (size_t i=0, n=fonts.size(); i<n; ++i)
                    unload_font_object(fonts.uget(i));
            }
        }
    }
}