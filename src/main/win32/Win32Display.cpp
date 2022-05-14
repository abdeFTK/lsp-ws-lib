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



namespace lsp
{
    namespace ws
    {
        namespace win32
        {

            Win32Display::Win32Display()
            {
                lsp_debug("Win32Display constructor");
                bExit           = false;
                hFtLibrary = NULL;
                root        = NULL;
                bzero(&sCairoUserDataKey, sizeof(sCairoUserDataKey));
            }

            Win32Display::~Win32Display()
            {
            }

            status_t Win32Display::init(int argc, const char **argv)
            {
                return STATUS_OK;
                //return IDisplay::init(argc, argv);
            }

            IWindow *Win32Display::create_window()
            {
                lsp_debug("CreateWindow");
                return new Win32Window(this, NULL, NULL, NULL, false);
            }

            IWindow *Win32Display::create_window(size_t screen)
            {
                lsp_debug("CreateWindow screen %ld", screen);
                return new Win32Window(this, NULL, NULL, root->hwnd, false);
            }

            IWindow *Win32Display::create_window(void *handle)
            {
                lsp_debug("CreateWindow with parent");
                lsp_trace("handle = %p", handle);
                root = new Win32Window(this, NULL, handle, NULL, false);
                return root;
            }

            IWindow *Win32Display::wrap_window(void *handle)
            {
                lsp_debug("Wrap Window with parent");
                root = new Win32Window(this, NULL, handle, NULL, true);
                return root;
            }

            ISurface *Win32Display::create_surface(size_t width, size_t height)
            {
                return new Win32CairoSurface(this, width, height);
            }

            void Win32Display::destroy()
            {
                IDisplay::destroy();
            }

            status_t Win32Display::main()
            {
                lsp_debug("Display MAIN");
                system::time_t ts;
                //MSG msg; 

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

                    // lsp_debug("PeekMessageW");
                    // // TODO pass root window in param
                    // while (PeekMessageW(&msg, NULL,  0, 0, PM_REMOVE)) 
                    // { 
                    //     lsp_debug("PeekMessageW %d", msg.message);
                    //     switch(msg.message) 
                    //     { 
                    //         case WM_DESTROY:
                    //         {
                    //             PostQuitMessage(0);
                    //             return STATUS_OK;
                    //         }
                    //         default:
                    //         {
                    //             TranslateMessage(&msg);
                    //             DispatchMessageW(&msg); 
                    //         }
                    //     } 
                    // }
                }

                return STATUS_OK;
            }

            status_t Win32Display::wait_events(wssize_t millis)
            {
                //IDisplay::wait_events(millis);
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
                if (!vWindows.premove(wnd))
                    return false;

                // Check if need to leave main cycle
                if (vWindows.size() <= 0) {
                    lsp_debug("leave main cycle");
                    bExit = true;
                }
                    
                return true;
            }

            void Win32Display::sync()
            { 
            }

            status_t Win32Display::main_iteration()
            {
                // Get current time to determine if need perform a rendering
                system::time_t ts;
                system::get_time(&ts);
                timestamp_t xts = (timestamp_t(ts.seconds) * 1000) + (ts.nanos / 1000000);

                // Do iteration
                return do_main_iteration(xts);
                //return STATUS_OK;
            }

            status_t Win32Display::do_main_iteration(timestamp_t ts)
            {
                status_t result = STATUS_OK;

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

                        //lsp_debug("Process task : %ld", t->nID);
                        // Process task
                        result  = t->pHandler(t->nTime, ts, t->pArg);
                        if (result != STATUS_OK)
                            break;
                    }
                }

                LPMSG msg; 

                system::time_t wmTs;
                // Get current time
                system::get_time(&wmTs);
                timestamp_t wmTsMsInitial     = (timestamp_t(wmTs.seconds) * 1000) + (wmTs.nanos / 1000000);
                timestamp_t wtime           = 50; // How many milliseconds to wait
                timestamp_t countDown       = 0;

                // if (hRootWnd == NULL)
                //     lsp_debug("Main iteration : hRootWnd is NULL");
                // else
                //     lsp_debug("Main iteration : hRootWnd is NOT NULL");

                // if (hRootWnd != NULL) { 
                //     while (countDown < wtime) 
                //     { 
                //         PeekMessageW(msg, hRootWnd,  0, 0, PM_NOREMOVE);
                //         DispatchMessageW(msg);
                //         system::get_time(&wmTs);
                //         timestamp_t wmTsMs     = (timestamp_t(wmTs.seconds) * 1000) + (wmTs.nanos / 1000000);
                //         countDown =  wmTsMs - wmTsMsInitial;
                //     }
                // }

                // lsp_debug("PeekMessage processed");

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
                return 1;
            }

            size_t Win32Display::default_screen()
            {
                return 1;
            }

            status_t Win32Display::screen_size(size_t screen, ssize_t *w, ssize_t *h)
            {
                if (screen == 1) {
                    if (w != NULL)
                        *w = root->width();
                    if (h != NULL)
                        *h = root->height();
                } else {
                    // TODO HWND GetDesktopWindow();
                    if (w != NULL)
                        *w = 1920;
                    if (h != NULL)
                        *h = 1080;
                }

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
                if (screen != NULL)
                    *screen = 1;
                if (left != NULL)
                    *left   = 10;
                if (top != NULL)
                    *top    = 10;
                return STATUS_OK;
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

                lsp_debug("Loading font '%s' from file '%s'", name, path->get_native());
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
                return STATUS_OK;
            }

            status_t Win32Display::remove_font(const char* name)
            {
                return STATUS_OK;
            }

            void Win32Display::remove_all_fonts()
            {
            }
        }
    }
}