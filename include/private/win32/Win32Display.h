#ifndef _UI_WIN32_WIN32DISPLAY_H_
#define _UI_WIN32_WIN32DISPLAY_H_

#include <lsp-plug.in/ws/version.h>
#include <lsp-plug.in/ws/IDisplay.h>

#include <lsp-plug.in/common/atomic.h>
#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/lltl/parray.h>
#include <lsp-plug.in/lltl/darray.h>
#include <lsp-plug.in/lltl/pphash.h>

#include <private/win32/Win32Window.h>

#include <cairo/cairo.h>
#include <ft2build.h>

#include FT_FREETYPE_H

namespace lsp
{
    namespace ws
    {
        namespace win32
        {
            class Win32Window;
            
            class Win32Display: public IDisplay
            {
                friend class Win32Window;

                public:

                    typedef struct font_t
                    {
                        char               *name;           // Name of the font
                        char               *alias;          // Font alias (the symbolic name of the font)
                        void               *data;           // Font data (font file contents loaded to memory)
                        ssize_t             refs;           // Number of references
                        FT_Face             ft_face;        // Font face handle for freetype

                    //#ifdef USE_LIBCAIRO
                        cairo_font_face_t  *cr_face[4];     // Font faces for cairo
                    //#endif /* USE_LIBCAIRO */
                    } font_t;


                    explicit Win32Display();
                    virtual ~Win32Display();

                    virtual status_t            init(int argc, const char **argv);
                    virtual void                destroy();

                public:

                    virtual status_t            main();
                    virtual status_t            main_iteration();
                    virtual void                quit_main();
                    virtual status_t            wait_events(wssize_t millis);

                    status_t                    do_main_iteration(timestamp_t ts);

                    virtual size_t              screens();
                    virtual size_t              default_screen();
                    virtual void                sync();
                    virtual status_t            screen_size(size_t screen, ssize_t *w, ssize_t *h);

                    virtual IWindow            *create_window();
                    virtual IWindow            *create_window(size_t screen);
                    virtual IWindow            *create_window(void *handle);
                    virtual IWindow            *wrap_window(void *handle);
                    virtual ISurface           *create_surface(size_t width, size_t height);

                    virtual status_t            set_clipboard(size_t id, IDataSource *ds);
                    virtual status_t            get_clipboard(size_t id, IDataSink *dst);
                    virtual const char * const *get_drag_ctypes();
                    virtual status_t            reject_drag();
                    virtual status_t            accept_drag(IDataSink *sink, drag_t action, bool internal, const rectangle_t *r);

                    virtual status_t            get_pointer_location(size_t *screen, ssize_t *left, ssize_t *top);
            
                
                    virtual status_t            add_font(const char *name, const char *path);
                    virtual status_t            add_font(const char *name, const io::Path *path);
                    virtual status_t            add_font(const char *name, const LSPString *path);
                    virtual status_t            add_font(const char *name, io::IInStream *is);
                    virtual status_t            add_font_alias(const char *name, const char *alias);
                    virtual status_t            remove_font(const char *name);
                    virtual void                remove_all_fonts();
                    void                        drop_custom_fonts();

                    status_t init_freetype_library();
                    static void     destroy_font_object(font_t *font);
                    static void     unload_font_object(font_t *font);
                    static font_t  *alloc_font_object(const char *name);
                    font_t                     *get_font(const char *name);

                    bool                        add_window(Win32Window *wnd);
                    bool                        remove_window(Win32Window *wnd);

                    /// Add window to the given grab group
                    status_t                    grab_events(Win32Window *wnd, grab_t group);
                    /// Remove window from grab group
                    status_t                    ungrab_events(Win32Window *wnd);
                    /// Prepare dispatch event. Save the current state of grab windows
                    bool prepare_dispatch(Win32Window* src);
                    /// Dispatch event to windows saved in the prepare state
                    /// Returns true if the event have been dispatched to other windows
                    bool dispatch_event(Win32Window* src, event_t *ev);
                    /// Find root window by screen number
                    Win32Window* findRootWindow(int screen);
                    /// Find first not used screen number (starts at 1)
                    int findFreeScreenNumber();

                    int findScreenOwner(HWND hwnd);

                    void setDroppedFile(const WCHAR* filename);

                    HANDLE                      get_cursor(mouse_pointer_t pointer);

                    const HMODULE GetCurrentModule();
                    const HMODULE GetCurrentDllModule();

                    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
                   

                    cairo_user_data_key_t       sCairoUserDataKey;
                    lltl::parray<Win32Window>     rootWindows;

                    HINSTANCE hInstance;

                    HGLOBAL hglbCopy;
                    LPWSTR  lptstrCopy; 
                    HGLOBAL   hglbPaste; 
                    LPWSTR    lptstrPaste; 
                    RECT dragRect;
                    
                    
                protected:
                    volatile bool               bExit;
                    lltl::darray<dtask_t>       sPending;
                    FT_Library                  hFtLibrary;
                    lltl::pphash<char, font_t>  vCustomFonts;
                    lltl::parray<Win32Window>     vWindows;
                    lltl::parray<Win32Window>     vGrab[__GRAB_TOTAL];
                    lltl::parray<Win32Window>     sTargets;
                    HANDLE                      vCursors[__MP_COUNT];
                    HICON mainIcon;

                private:
                    LSPString droppedFile;
                    
            };
        }
    }
}


#endif /* UI_WIN32_WIN32DISPLAY_H_ */