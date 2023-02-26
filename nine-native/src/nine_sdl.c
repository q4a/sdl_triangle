// ----------------------------------------
// nine_sdl

#include "nine_sdl.h"

#include <dlfcn.h>
#include <malloc.h>
#include <memory.h>
#include <fcntl.h>
#include <time.h>

#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <X11/Xlib-xcb.h>

#include <d3dadapter/drm.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "dri3.h"

// ----------- utils funcs / stubs ------------------------
#define TRACE(...)
#define FIXME(...)   fprintf(stderr, __VA_ARGS__)
#define WARN(...)   fprintf(stderr, __VA_ARGS__)
#define ERR(...)    fprintf(stderr, __VA_ARGS__)

static inline BOOL IsEqualGUID(const GUID* a, const GUID* b)
{
    return memcmp(a,b,sizeof(GUID)) == 0;
}
static const char* debugstr_guid(const GUID* id)
{
    return "GUID";
}

static inline LONG WINAPI InterlockedIncrement( LONG volatile *dest )
{
    return __sync_add_and_fetch(dest, 1);;
}
static inline LONG WINAPI InterlockedDecrement( LONG volatile *dest )
{
    return __sync_sub_and_fetch(dest, 1);;
}


// ---------------------------------------------------------------------------------------

static const D3DFORMAT ConvertFromSDL(Uint32 format)
{
    switch (format)
    {
    case SDL_PIXELFORMAT_ARGB4444:      return D3DFMT_A4R4G4B4;
    case SDL_PIXELFORMAT_RGB332:        return D3DFMT_R3G3B2;
    case SDL_PIXELFORMAT_ARGB1555:      return D3DFMT_A1R5G5B5;
    case SDL_PIXELFORMAT_RGB555:        return D3DFMT_X1R5G5B5;
    case SDL_PIXELFORMAT_RGB565:        return D3DFMT_R5G6B5;
    case SDL_PIXELFORMAT_RGB24:         return D3DFMT_R8G8B8;
    case SDL_PIXELFORMAT_RGB888:        return D3DFMT_X8R8G8B8;
    case SDL_PIXELFORMAT_ARGB8888:      return D3DFMT_A8R8G8B8;
    case SDL_PIXELFORMAT_ARGB2101010:   return D3DFMT_A2R10G10B10;
    default:
    case SDL_PIXELFORMAT_UNKNOWN:       return D3DFMT_UNKNOWN;
    }
}


static const Uint32 ConvertToSDL(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_A4R4G4B4:     return SDL_PIXELFORMAT_ARGB4444;
    case D3DFMT_R3G3B2:       return SDL_PIXELFORMAT_RGB332;
    case D3DFMT_A1R5G5B5:     return SDL_PIXELFORMAT_ARGB1555;
    case D3DFMT_X1R5G5B5:     return SDL_PIXELFORMAT_RGB555;
    case D3DFMT_R5G6B5:       return SDL_PIXELFORMAT_RGB565;
    case D3DFMT_R8G8B8:       return SDL_PIXELFORMAT_RGB24;
    case D3DFMT_X8R8G8B8:     return SDL_PIXELFORMAT_RGB888;
    case D3DFMT_A8R8G8B8:     return SDL_PIXELFORMAT_ARGB8888;
    case D3DFMT_A2R10G10B10:  return SDL_PIXELFORMAT_ARGB2101010;
    default:
    case D3DFMT_UNKNOWN:      return SDL_PIXELFORMAT_UNKNOWN;
    }
}


// ---- dlls/winex11.drv/d3dadapter.c ----------------------------------------------------------------

#include <libdrm/drm.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>

const GUID IID_IDirect3D9Ex = { 0x02177241, 0x69FC, 0x400C, {0x8F, 0xF1, 0x93, 0xA4, 0x4D, 0xF6, 0x86, 0x1D}};
const GUID IID_IDirect3D9 = { 0x81BDCBCA, 0x64D4, 0x426D, {0xAE, 0x8D, 0xAD, 0x1, 0x47, 0xF4, 0x27, 0x5C}};
const GUID IID_ID3DPresent = { 0x77D60E80, 0xF1E6, 0x11DF, { 0x9E, 0x39, 0x95, 0x0C, 0xDF, 0xD7, 0x20, 0x85 } };
const GUID IID_ID3DPresentGroup = { 0xB9C3016E, 0xF32A, 0x11DF, { 0x9C, 0x18, 0x92, 0xEA, 0xDE, 0xD7, 0x20, 0x85 } };
const GUID IID_IUnknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };


struct DRI3Present
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    LONG refs;

    D3DPRESENT_PARAMETERS params;
    PRESENTpriv *present_priv;

    SDL_Window* sdl_win;
    Display*    x11_display;
    Window      x11_window;
};

struct D3DWindowBuffer
{
    PRESENTPixmapPriv *present_pixmap_priv;
};


static ULONG WINAPI
DRI3Present_AddRef( struct DRI3Present *This )
{
    ULONG refs = InterlockedIncrement(&This->refs);
    TRACE("%p increasing refcount to %u.\n", This, refs);
    return refs;
}

static ULONG WINAPI
DRI3Present_Release( struct DRI3Present *This )
{
    ULONG refs = InterlockedDecrement(&This->refs);
    TRACE("%p decreasing refcount to %u.\n", This, refs);
    if (refs == 0) {
        /* dtor */
        SDL_SetWindowFullscreen(This->sdl_win, FALSE);
        PRESENTDestroy(This->x11_display, This->present_priv);
        free(This);
    }
    return refs;
}

static HRESULT WINAPI
DRI3Present_QueryInterface( struct DRI3Present *This,
                            REFIID riid,
                            void **ppvObject )
{
    if (!ppvObject) { return E_POINTER; }

    if (IsEqualGUID(&IID_ID3DPresent, riid) ||
        IsEqualGUID(&IID_IUnknown, riid)) {
        *ppvObject = This;
        DRI3Present_AddRef(This);
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(riid));
    *ppvObject = NULL;

    return E_NOINTERFACE;
}

static void
DRI3Present_ChangePresentParameters( struct DRI3Present *This,
                                    D3DPRESENT_PARAMETERS *params,
                                    BOOL first_time);

static HRESULT WINAPI
DRI3Present_SetPresentParameters( struct DRI3Present *This,
                                  D3DPRESENT_PARAMETERS *pPresentationParameters,
                                  D3DDISPLAYMODEEX *pFullscreenDisplayMode )
{
    if (!pPresentationParameters) {
        WARN("pPresentationParameters is NULL.\n");
        return D3DERR_INVALIDCALL;
    }

    if (pPresentationParameters->Windowed) {
        SDL_SetWindowFullscreen(This->sdl_win, FALSE);
        DRI3Present_ChangePresentParameters(This, pPresentationParameters, FALSE);

    }
    else {
        if (!pFullscreenDisplayMode) {
            WARN("pFullscreenDisplayMode is NULL.\n");
            return D3DERR_INVALIDCALL;
        }

        SDL_DisplayMode target;
        SDL_DisplayMode closest;
        memset(&target, 0, sizeof(target));
        memset(&closest, 0, sizeof(closest));

        // msdn: "When switching to full-screen mode,
        //  Direct3D will try to find a desktop format that matches the back buffer format,
        //  so that back buffer and front buffer formats will be identical (to eliminate the need for color conversion)."
        Uint32 preferred_format_for_backbuffer = ConvertToSDL(pPresentationParameters->BackBufferFormat);

        target.w = pFullscreenDisplayMode->Height;
        target.h = pFullscreenDisplayMode->Width;
        target.refresh_rate = pFullscreenDisplayMode->RefreshRate;
        if (preferred_format_for_backbuffer != SDL_PIXELFORMAT_UNKNOWN)
            target.format = preferred_format_for_backbuffer;
        else
            target.format = ConvertToSDL(pFullscreenDisplayMode->Format);

        SDL_DisplayMode* mode = NULL;
        if (FALSE) {
            /*
             * this doesn't seem to be a good idea:
             *       it returns different mode even when the request mode exits and works...
             */
            int Adapter = 0;
            mode = SDL_GetClosestDisplayMode(Adapter, &target, &closest);
            if (!mode) {
                WARN("Could not find requested fullscreen display mode (%dx%d %dHz, format = %d).\n", pFullscreenDisplayMode->Width, pFullscreenDisplayMode->Height, pFullscreenDisplayMode->RefreshRate, pFullscreenDisplayMode->Format);
            }
        }
        else {
            mode = &target;
        }

        int err = SDL_SetWindowDisplayMode(This->sdl_win, mode);
        if (err < 0) {
            WARN("SDL_SetWindowDisplayMode returned an error: %s\n", SDL_GetError());
            return D3DERR_INVALIDCALL;
        }

        err = SDL_SetWindowFullscreen(This->sdl_win, SDL_WINDOW_FULLSCREEN);
        if (err < 0) {
            WARN("SDL_SetWindowFullscreen returned an error: %s\n", SDL_GetError());
            return D3DERR_INVALIDCALL;
        }

        DRI3Present_ChangePresentParameters(This, pPresentationParameters, FALSE);
    }
    return D3D_OK;
}

static HRESULT WINAPI
DRI3Present_D3DWindowBufferFromDmaBuf( struct DRI3Present *This,
                       int dmaBufFd,
                       int width,
                       int height,
                       int stride,
                       int depth,
                       int bpp,
                       struct D3DWindowBuffer **out)
{
    Pixmap pixmap;

    if (!DRI3PixmapFromDmaBuf(This->x11_display, DefaultScreen(This->x11_display),
                              dmaBufFd, width, height, stride, depth,
                              bpp, &pixmap ))
        return D3DERR_DRIVERINTERNALERROR;

    *out = calloc(1, sizeof(struct D3DWindowBuffer));
    PRESENTPixmapInit(This->present_priv, pixmap, &((*out)->present_pixmap_priv));
    return D3D_OK;
}

static HRESULT WINAPI
DRI3Present_DestroyD3DWindowBuffer( struct DRI3Present *This,
                           struct D3DWindowBuffer *buffer )
{
    /* the pixmap is managed by the PRESENT backend.
     * But if it can delete it right away, we may have
     * better performance */
    PRESENTTryFreePixmap(This->x11_display, buffer->present_pixmap_priv);
    free(buffer);
    return D3D_OK;
}

static HRESULT WINAPI
DRI3Present_WaitBufferReleased( struct DRI3Present *This,
                                struct D3DWindowBuffer *buffer)
{
    (void) This;
    PRESENTWaitPixmapReleased(buffer->present_pixmap_priv);
    return D3D_OK;
}

static HRESULT WINAPI
DRI3Present_FrontBufferCopy( struct DRI3Present *This,
                             struct D3DWindowBuffer *buffer )
{
    /* TODO: use dc_rect */
    if (PRESENTHelperCopyFront(This->x11_display, buffer->present_pixmap_priv))
        return D3D_OK;
    else
        return D3DERR_DRIVERINTERNALERROR;
}

static HRESULT WINAPI
DRI3Present_PresentBuffer( struct DRI3Present *This,
                           struct D3DWindowBuffer *buffer,
                           HWND hWndOverride,
                           const RECT *pSourceRect,
                           const RECT *pDestRect,
                           const RGNDATA *pDirtyRegion,
                           DWORD Flags )
{
/*   TODO?

    if (d3d->dc_rect.top != 0 &&
        d3d->dc_rect.left != 0) {
        if (!pDestRect)
            pDestRect = (const RECT *) &(d3d->dc_rect);
        else {
            dest_translate.top = pDestRect->top + d3d->dc_rect.top;
            dest_translate.left = pDestRect->left + d3d->dc_rect.left;
            dest_translate.bottom = pDestRect->bottom + d3d->dc_rect.bottom;
            dest_translate.right = pDestRect->right + d3d->dc_rect.right;
            pDestRect = (const RECT *) &dest_translate;
        }
    }
*/

    if (!PRESENTPixmap(This->x11_display, This->x11_window, buffer->present_pixmap_priv,
                       &This->params, pSourceRect, pDestRect, pDirtyRegion))
        return D3DERR_DRIVERINTERNALERROR;

    return D3D_OK;
}

static HRESULT WINAPI
DRI3Present_GetRasterStatus( struct DRI3Present *This,
                             D3DRASTER_STATUS *pRasterStatus )
{
    FIXME("(%p, %p), stub!\n", This, pRasterStatus);
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
DRI3Present_GetDisplayMode( struct DRI3Present *This,
                            D3DDISPLAYMODEEX *pMode,
                            D3DDISPLAYROTATION *pRotation )
{
    int Adapter = 0;
    SDL_DisplayMode mode;
    int err = SDL_GetDesktopDisplayMode(Adapter, &mode);
    if (err < 0) {
        WARN("SDL_GetCurrentDisplayMode returned an error: %s\n", SDL_GetError());
        return D3DERR_INVALIDCALL;
    }

    pMode->Width = mode.w;
    pMode->Height = mode.h;
    pMode->RefreshRate = mode.refresh_rate;
    pMode->Format = ConvertFromSDL(mode.format);
    pMode->ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;

    *pRotation = D3DDISPLAYROTATION_IDENTITY;

    return D3D_OK;
}

static HRESULT WINAPI
DRI3Present_GetPresentStats( struct DRI3Present *This,
                             D3DPRESENTSTATS *pStats )
{
    FIXME("(%p, %p), stub!\n", This, pStats);
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
DRI3Present_GetCursorPos( struct DRI3Present *This,
                          POINT *pPoint )
{
    // FIXME faked
    BOOL ok;
    if (!pPoint)
        return D3DERR_INVALIDCALL;
    ok = TRUE;
    memset(pPoint,0,sizeof(POINT));
    return ok ? S_OK : D3DERR_DRIVERINTERNALERROR;
}

static HRESULT WINAPI
DRI3Present_SetCursorPos( struct DRI3Present *This,
                          POINT *pPoint )
{
    // FIXME faked
    if (!pPoint)
        return D3DERR_INVALIDCALL;
    return S_OK;
}

static HRESULT WINAPI
DRI3Present_SetCursor( struct DRI3Present *This,
                       void *pBitmap,
                       POINT *pHotspot,
                       BOOL bShow )
{
    // FIXME faked
   return D3D_OK;
}

static HRESULT WINAPI
DRI3Present_SetGammaRamp( struct DRI3Present *This,
                          const D3DGAMMARAMP *pRamp,
                          HWND hWndOverride )
{
    // FIXME faked
    return D3D_OK;
}

static HRESULT WINAPI
DRI3Present_GetWindowInfo( struct DRI3Present *This,
                           HWND hWnd,
                           int *width, int *height, int *depth )
{
    int w,h;
    SDL_GetWindowSize(This->sdl_win, &w, &h);    
    Uint32 format = SDL_GetWindowPixelFormat(This->sdl_win);

    *width = w;
    *height = h;
    *depth = format != SDL_PIXELFORMAT_UNKNOWN ? SDL_BITSPERPIXEL(format) : 24;
    return D3D_OK;
}

/*----------*/


static ID3DPresentVtbl DRI3Present_vtable = {
    (void *)DRI3Present_QueryInterface,
    (void *)DRI3Present_AddRef,
    (void *)DRI3Present_Release,
    (void *)DRI3Present_SetPresentParameters,
    (void *)DRI3Present_D3DWindowBufferFromDmaBuf,
    (void *)DRI3Present_DestroyD3DWindowBuffer,
    (void *)DRI3Present_WaitBufferReleased,
    (void *)DRI3Present_FrontBufferCopy,
    (void *)DRI3Present_PresentBuffer,
    (void *)DRI3Present_GetRasterStatus,
    (void *)DRI3Present_GetDisplayMode,
    (void *)DRI3Present_GetPresentStats,
    (void *)DRI3Present_GetCursorPos,
    (void *)DRI3Present_SetCursorPos,
    (void *)DRI3Present_SetCursor,
    (void *)DRI3Present_SetGammaRamp,
    (void *)DRI3Present_GetWindowInfo
};

static void
DRI3Present_ChangePresentParameters( struct DRI3Present *This,
                                    D3DPRESENT_PARAMETERS *params,
                                    BOOL first_time)
{
    (void) first_time; /* will be used to manage screen res if windowed mode change */

    if (params->hDeviceWindow && params->hDeviceWindow != This->sdl_win) {
        WARN("Changing hDeviceWindow not supported\n");
    }

    int w,h;
    SDL_GetWindowSize(This->sdl_win, &w, &h);
    params->BackBufferWidth = w;
    params->BackBufferHeight = h;

    This->params = *params;
}

static HRESULT
DRI3Present_new( SDL_Window* sdl_win,
                 D3DPRESENT_PARAMETERS *params,
                 struct DRI3Present **out )
{
    struct DRI3Present *This;

    if (!sdl_win) {
        ERR("No SDL_Window specified for presentation backend.\n");
        return D3DERR_INVALIDCALL;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_bool Ok = SDL_GetWindowWMInfo(sdl_win, &info);
    if (!Ok) {
        ERR("Invalid SDL_Window specified for presentation backend.\n");
        return D3DERR_INVALIDCALL;
    }

    This = calloc(1, sizeof(struct DRI3Present));
    if (!This) {
        return E_OUTOFMEMORY;
    }

    This->vtable = &DRI3Present_vtable;
    This->refs = 1;
    This->sdl_win = sdl_win;
    This->x11_display = info.info.x11.display;
    This->x11_window = info.info.x11.window;

    DRI3Present_ChangePresentParameters(This, params, TRUE);

    PRESENTInit(info.info.x11.display, &(This->present_priv));

    *out = This;

    return D3D_OK;
}

struct DRI3PresentGroup
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    LONG refs;

    struct DRI3Present **present_backends;
    unsigned npresent_backends;
};

static ULONG WINAPI
DRI3PresentGroup_AddRef( struct DRI3PresentGroup *This )
{
    ULONG refs = InterlockedIncrement(&This->refs);
    TRACE("%p increasing refcount to %u.\n", This, refs);
    return refs;
}

static ULONG WINAPI
DRI3PresentGroup_Release( struct DRI3PresentGroup *This )
{
    ULONG refs = InterlockedDecrement(&This->refs);
    TRACE("%p decreasing refcount to %u.\n", This, refs);
    if (refs == 0) {
        unsigned i;
        if (This->present_backends) {
            for (i = 0; i < This->npresent_backends; ++i) {
                if (This->present_backends[i])
                    DRI3Present_Release(This->present_backends[i]);
            }
            free(This->present_backends);
        }
        free(This);
    }
    return refs;
}

static HRESULT WINAPI
DRI3PresentGroup_QueryInterface( struct DRI3PresentGroup *This,
                                 REFIID riid,
                                 void **ppvObject )
{
    if (!ppvObject) { return E_POINTER; }
    if (IsEqualGUID(&IID_ID3DPresentGroup, riid) ||
        IsEqualGUID(&IID_IUnknown, riid)) {
        *ppvObject = This;
        DRI3PresentGroup_AddRef(This);
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(riid));
    *ppvObject = NULL;

    return E_NOINTERFACE;
}

static UINT WINAPI
DRI3PresentGroup_GetMultiheadCount( struct DRI3PresentGroup *This )
{
    FIXME("(%p), stub!\n", This);
    return 1;
}

static HRESULT WINAPI
DRI3PresentGroup_GetPresent( struct DRI3PresentGroup *This,
                             UINT Index,
                             ID3DPresent **ppPresent )
{
    if (Index >= DRI3PresentGroup_GetMultiheadCount(This)) {
        ERR("Index >= MultiHeadCount\n");
        return D3DERR_INVALIDCALL;
    }
    DRI3Present_AddRef(This->present_backends[Index]);
    *ppPresent = (ID3DPresent *)This->present_backends[Index];

    return D3D_OK;
}

static HRESULT WINAPI
DRI3PresentGroup_CreateAdditionalPresent( struct DRI3PresentGroup *This,
                                          D3DPRESENT_PARAMETERS *pPresentationParameters,
                                          ID3DPresent **ppPresent )
{
    FIXME("(%p, %p, %p), stub!\n", This, pPresentationParameters, ppPresent);
    return D3DERR_INVALIDCALL;
}

static void WINAPI
DRI3PresentGroup_GetVersion( struct DRI3PresentGroup *This,
                             int *major,
                             int *minor)
{
    *major = 1;
    *minor = 0;
}

static ID3DPresentGroupVtbl DRI3PresentGroup_vtable = {
    (void *)DRI3PresentGroup_QueryInterface,
    (void *)DRI3PresentGroup_AddRef,
    (void *)DRI3PresentGroup_Release,
    (void *)DRI3PresentGroup_GetMultiheadCount,
    (void *)DRI3PresentGroup_GetPresent,
    (void *)DRI3PresentGroup_CreateAdditionalPresent,
    (void *)DRI3PresentGroup_GetVersion
};

static HRESULT
dri3_create_present_group( SDL_Window* win,
                           UINT adapter,
                           D3DPRESENT_PARAMETERS *params,
                           unsigned nparams,
                           ID3DPresentGroup **group )
{
    struct DRI3PresentGroup *This = calloc(1, sizeof(struct DRI3PresentGroup));
    HRESULT hr;
    unsigned i;

    if (!This) {
        ERR("Out of memory.\n");
        return E_OUTOFMEMORY;
    }

    This->vtable = &DRI3PresentGroup_vtable;
    This->refs = 1;
    This->npresent_backends = nparams;
    This->present_backends = calloc(This->npresent_backends, sizeof(struct DRI3Present *));

    if (!This->present_backends) {
        DRI3PresentGroup_Release(This);
        ERR("Out of memory.\n");
        return E_OUTOFMEMORY;
    }

    if (nparams != 1) { adapter = 0; }
    for (i = 0; i < This->npresent_backends; ++i) {
         /* create an ID3DPresent for it */
        hr = DRI3Present_new(win, &params[i],
                             &This->present_backends[i]);
        if (FAILED(hr)) {
            DRI3PresentGroup_Release(This);
            return hr;
        }
    }

    *group = (ID3DPresentGroup *)This;
    TRACE("Returning %p\n", *group);

    return D3D_OK;
}



// --------- dlls/d3d9/d3dadapter9.c ----------------------------------------------------------------

struct d3dadapter9
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    LONG refs;

    /* simple test, one adapter */
    ID3DAdapter9 *adapter;

    /* true if it implements IDirect3D9Ex */
    BOOL ex;
};

/* convenience wrapper for calls into ID3D9Adapter */
#define ADAPTER_PROC(name, ...) \
    ID3DAdapter9_##name(This->adapter, ## __VA_ARGS__)

static HRESULT WINAPI
d3dadapter9_CheckDeviceFormat( struct d3dadapter9 *This,
                               UINT Adapter,
                               D3DDEVTYPE DeviceType,
                               D3DFORMAT AdapterFormat,
                               DWORD Usage,
                               D3DRESOURCETYPE RType,
                               D3DFORMAT CheckFormat );

static ULONG WINAPI
d3dadapter9_AddRef( struct d3dadapter9 *This )
{
    ULONG refs = InterlockedIncrement(&This->refs);
    TRACE("%p increasing refcount to %u.\n", This, refs);
    return refs;
}

static ULONG WINAPI
d3dadapter9_Release( struct d3dadapter9 *This )
{
    ULONG refs = InterlockedDecrement(&This->refs);
    TRACE("%p decreasing refcount to %u.\n", This, refs);
    if (refs == 0) {
        /* dtor */
        if (This->adapter){
            ID3DAdapter9_Release(This->adapter);
        }

        free(This);
    }
    return refs;
}

static HRESULT WINAPI
d3dadapter9_QueryInterface( struct d3dadapter9 *This,
                            REFIID riid,
                            void **ppvObject )
{
    if (!ppvObject) { return E_POINTER; }
    if ((IsEqualGUID(&IID_IDirect3D9Ex, riid) && This->ex) ||
         IsEqualGUID(&IID_IDirect3D9, riid) ||
         IsEqualGUID(&IID_IUnknown, riid)) {
        *ppvObject = This;
        d3dadapter9_AddRef(This);
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(riid));
    *ppvObject = NULL;

    return E_NOINTERFACE;
}

static HRESULT WINAPI
d3dadapter9_RegisterSoftwareDevice( struct d3dadapter9 *This,
                                    void *pInitializeFunction )
{
    FIXME("(%p, %p), stub!\n", This, pInitializeFunction);
    return D3DERR_INVALIDCALL;
}

static UINT WINAPI
d3dadapter9_GetAdapterCount( struct d3dadapter9 *This )
{
    return This ? 1 : 0;
}

static HRESULT WINAPI
d3dadapter9_GetAdapterIdentifier( struct d3dadapter9 *This,
                                  UINT Adapter,
                                  DWORD Flags,
                                  D3DADAPTER_IDENTIFIER9 *pIdentifier )
{
    HRESULT hr;

    if (Adapter >= d3dadapter9_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }

    hr = ADAPTER_PROC(GetAdapterIdentifier, Flags, pIdentifier);
    return hr;
}

static UINT WINAPI
d3dadapter9_GetAdapterModeCount( struct d3dadapter9 *This,
                                 UINT Adapter,
                                 D3DFORMAT Format )
{
    if (Adapter >= d3dadapter9_GetAdapterCount(This)) {
        WARN("Adapter %u does not exist.\n", Adapter);
        return 0;
    }
    if (FAILED(d3dadapter9_CheckDeviceFormat(This, Adapter, D3DDEVTYPE_HAL,
                                         Format, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_SURFACE, Format))) {
        WARN("DeviceFormat not available.\n");
        return 0;
    }

    int NumMatchingModes = 0;
    int NumModes = SDL_GetNumDisplayModes(Adapter);
    int i;
    for (i=0;i<NumModes;i++) {
        SDL_DisplayMode mode;
        int err = SDL_GetDisplayMode(Adapter, i, &mode);
        if (err < 0) {
            WARN("SDL_GetDisplayMode returned an error: %s\n", SDL_GetError());
            return 0;
        }

        if (Format == ConvertFromSDL(mode.format))
            NumMatchingModes ++;
    }

    return NumMatchingModes;
}

static HRESULT WINAPI
d3dadapter9_EnumAdapterModes( struct d3dadapter9 *This,
                              UINT Adapter,
                              D3DFORMAT Format,
                              UINT ModeIndex,
                              D3DDISPLAYMODE *pMode )
{
    HRESULT hr;

    if (Adapter >= d3dadapter9_GetAdapterCount(This)) {
        WARN("Adapter %u does not exist.\n", Adapter);
        return D3DERR_INVALIDCALL;
    }
    if (!pMode) {
        WARN("pMode is NULL.\n");
        return D3DERR_INVALIDCALL;
    }

    hr = d3dadapter9_CheckDeviceFormat(This, Adapter, D3DDEVTYPE_HAL,
                                   Format, D3DUSAGE_RENDERTARGET,
                                   D3DRTYPE_SURFACE, Format);
    if (FAILED(hr)) {
        TRACE("DeviceFormat not available.\n");
        return hr;
    }

    int IndexMatchingModes = 0;
    int NumModes = SDL_GetNumDisplayModes(Adapter);
    int i;
    for (i=0;i<NumModes;i++) {
        SDL_DisplayMode mode;
        int err = SDL_GetDisplayMode(Adapter, i, &mode);
        if (err < 0) {
            WARN("SDL_GetDisplayMode returned an error: %s\n", SDL_GetError());
            return D3DERR_INVALIDCALL;
        }

        if (Format != ConvertFromSDL(mode.format))
            continue;

        if (IndexMatchingModes == ModeIndex) {
            TRACE("DiplayMode: %dx%d@%dHz, d3dformat=%d\n", mode.w, mode.h, mode.refresh_rate, Format);
            pMode->Width = mode.w;
            pMode->Height = mode.h;
            pMode->RefreshRate = mode.refresh_rate;
            pMode->Format = Format;

            return D3D_OK;
        }
        IndexMatchingModes ++;
    }

    WARN("invalid mode for format %d on adapter %d: %d\n", Format, Adapter, ModeIndex);
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
d3dadapter9_GetAdapterDisplayMode( struct d3dadapter9 *This,
                                   UINT Adapter,
                                   D3DDISPLAYMODE *pMode )
{
    if (Adapter >= d3dadapter9_GetAdapterCount(This)) {
        WARN("Adapter %u does not exist.\n", Adapter);
        return D3DERR_INVALIDCALL;
    }

    SDL_DisplayMode mode;
    int err = SDL_GetCurrentDisplayMode(Adapter, &mode);
    if (err < 0) {
        WARN("SDL_GetCurrentDisplayMode returned an error: %s\n", SDL_GetError());
        return D3DERR_INVALIDCALL;
    }

    pMode->Width = mode.w;
    pMode->Height = mode.h;
    pMode->RefreshRate = mode.refresh_rate;
    pMode->Format = ConvertFromSDL(mode.format);

    return D3D_OK;
}

static HRESULT WINAPI
d3dadapter9_CheckDeviceType( struct d3dadapter9 *This,
                             UINT Adapter,
                             D3DDEVTYPE DevType,
                             D3DFORMAT AdapterFormat,
                             D3DFORMAT BackBufferFormat,
                             BOOL bWindowed )
{
    if (Adapter >= d3dadapter9_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDeviceType,
                        DevType, AdapterFormat, BackBufferFormat, bWindowed);
}

static HRESULT WINAPI
d3dadapter9_CheckDeviceFormat( struct d3dadapter9 *This,
                               UINT Adapter,
                               D3DDEVTYPE DeviceType,
                               D3DFORMAT AdapterFormat,
                               DWORD Usage,
                               D3DRESOURCETYPE RType,
                               D3DFORMAT CheckFormat )
{
    if (Adapter >= d3dadapter9_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDeviceFormat,
                        DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}

static HRESULT WINAPI
d3dadapter9_CheckDeviceMultiSampleType( struct d3dadapter9 *This,
                                        UINT Adapter,
                                        D3DDEVTYPE DeviceType,
                                        D3DFORMAT SurfaceFormat,
                                        BOOL Windowed,
                                        D3DMULTISAMPLE_TYPE MultiSampleType,
                                        DWORD *pQualityLevels )
{
    if (Adapter >= d3dadapter9_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDeviceMultiSampleType, DeviceType, SurfaceFormat,
                        Windowed, MultiSampleType, pQualityLevels);
}

static HRESULT WINAPI
d3dadapter9_CheckDepthStencilMatch( struct d3dadapter9 *This,
                                    UINT Adapter,
                                    D3DDEVTYPE DeviceType,
                                    D3DFORMAT AdapterFormat,
                                    D3DFORMAT RenderTargetFormat,
                                    D3DFORMAT DepthStencilFormat )
{
    if (Adapter >= d3dadapter9_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDepthStencilMatch, DeviceType, AdapterFormat,
                        RenderTargetFormat, DepthStencilFormat);
}

static HRESULT WINAPI
d3dadapter9_CheckDeviceFormatConversion( struct d3dadapter9 *This,
                                     UINT Adapter,
                                     D3DDEVTYPE DeviceType,
                                     D3DFORMAT SourceFormat,
                                     D3DFORMAT TargetFormat )
{
    if (Adapter >= d3dadapter9_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDeviceFormatConversion,
                        DeviceType, SourceFormat, TargetFormat);
}

static HRESULT WINAPI
d3dadapter9_GetDeviceCaps( struct d3dadapter9 *This,
                           UINT Adapter,
                           D3DDEVTYPE DeviceType,
                           D3DCAPS9 *pCaps )
{
    HRESULT hr;

    if (Adapter >= d3dadapter9_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }

    hr = ADAPTER_PROC(GetDeviceCaps, DeviceType, pCaps);
    if (FAILED(hr)) { return hr; }

    pCaps->MasterAdapterOrdinal = 0;
    pCaps->AdapterOrdinalInGroup = 0;
    pCaps->NumberOfAdaptersInGroup = 1;

    return hr;
}

static HMONITOR WINAPI
d3dadapter9_GetAdapterMonitor( struct d3dadapter9 *This,
                               UINT Adapter )
{
    // FIXME faked
    if (Adapter >= d3dadapter9_GetAdapterCount(This)) { return (HMONITOR)0; }
    return (HMONITOR)0;;
}

static HRESULT WINAPI
d3dadapter9_CreateDeviceEx( struct d3dadapter9 *This,
                            UINT Adapter,
                            D3DDEVTYPE DeviceType,
                            HWND hFocusWindow,
                            DWORD BehaviorFlags,
                            D3DPRESENT_PARAMETERS *pPresentationParameters,
                            D3DDISPLAYMODEEX *pFullscreenDisplayMode,
                            IDirect3DDevice9Ex **ppReturnedDeviceInterface );

static HRESULT WINAPI
d3dadapter9_CreateDevice( struct d3dadapter9 *This,
                          UINT Adapter,
                          D3DDEVTYPE DeviceType,
                          HWND hFocusWindow,
                          DWORD BehaviorFlags,
                          D3DPRESENT_PARAMETERS *pPresentationParameters,
                          IDirect3DDevice9 **ppReturnedDeviceInterface )
{
    HRESULT hr;
    hr = d3dadapter9_CreateDeviceEx(This, Adapter, DeviceType, hFocusWindow,
                                    BehaviorFlags, pPresentationParameters,
                                    NULL,
                                    (IDirect3DDevice9Ex **)ppReturnedDeviceInterface);
    if (FAILED(hr))
        return hr;
    return D3D_OK;
}

static UINT WINAPI
d3dadapter9_GetAdapterModeCountEx( struct d3dadapter9 *This,
                                   UINT Adapter,
                                   const D3DDISPLAYMODEFILTER *pFilter )
{
    return 1;
}

static HRESULT WINAPI
d3dadapter9_EnumAdapterModesEx( struct d3dadapter9 *This,
                                UINT Adapter,
                                const D3DDISPLAYMODEFILTER *pFilter,
                                UINT Mode,
                                D3DDISPLAYMODEEX *pMode )
{
    FIXME("(%p, %u, %p, %u, %p), stub!\n", This, Adapter, pFilter, Mode, pMode);
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
d3dadapter9_GetAdapterDisplayModeEx( struct d3dadapter9 *This,
                                     UINT Adapter,
                                     D3DDISPLAYMODEEX *pMode,
                                     D3DDISPLAYROTATION *pRotation )
{
    FIXME("(%p, %u, %p, %p), stub!\n", This, Adapter, pMode, pRotation);
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
d3dadapter9_CreateDeviceEx( struct d3dadapter9 *This,
                            UINT Adapter,
                            D3DDEVTYPE DeviceType,
                            HWND hFocusWindow,
                            DWORD BehaviorFlags,
                            D3DPRESENT_PARAMETERS *pPresentationParameters,
                            D3DDISPLAYMODEEX *pFullscreenDisplayMode,
                            IDirect3DDevice9Ex **ppReturnedDeviceInterface )
{
    ID3DPresentGroup *present;
    HRESULT hr;
    SDL_Window* sdl_win = (SDL_Window*)hFocusWindow;

    if (Adapter >= d3dadapter9_GetAdapterCount(This)) {
        WARN("Adapter %u does not exist.\n", Adapter);
        return D3DERR_INVALIDCALL;
    }

    if (!sdl_win) {
        ERR("no SDL_Window specified\n");
        return D3DERR_INVALIDCALL;
    }

    {
        UINT nparams = 1;
        UINT ordinal = 0;
        hr = dri3_create_present_group(sdl_win, ordinal, pPresentationParameters,
                                               nparams, &present);
    }

    if (FAILED(hr)) {
        WARN("Failed to create PresentGroup.\n");
        return hr;
    }

    if (This->ex) {
        hr = ADAPTER_PROC(CreateDeviceEx, Adapter, DeviceType, hFocusWindow,
                          BehaviorFlags, pPresentationParameters,
                          pFullscreenDisplayMode,
                          (IDirect3D9Ex *)This, present,
                          ppReturnedDeviceInterface);
    } else { /* CreateDevice on non-ex */
        hr = ADAPTER_PROC(CreateDevice, Adapter, DeviceType, hFocusWindow,
                          BehaviorFlags, pPresentationParameters,
                          (IDirect3D9 *)This, present,
                          (IDirect3DDevice9 **)ppReturnedDeviceInterface);
    }
    if (FAILED(hr)) {
        WARN("ADAPTER_PROC failed.\n");
        ID3DPresentGroup_Release(present);
    }

    return hr;
}

static HRESULT WINAPI
d3dadapter9_GetAdapterLUID( struct d3dadapter9 *This,
                            UINT Adapter,
                            LUID *pLUID )
{
    FIXME("(%p, %u, %p), stub!\n", This, Adapter, pLUID);
    return D3DERR_INVALIDCALL;
}

static IDirect3D9ExVtbl d3dadapter9_vtable = {
    (void *)d3dadapter9_QueryInterface,
    (void *)d3dadapter9_AddRef,
    (void *)d3dadapter9_Release,
    (void *)d3dadapter9_RegisterSoftwareDevice,
    (void *)d3dadapter9_GetAdapterCount,
    (void *)d3dadapter9_GetAdapterIdentifier,
    (void *)d3dadapter9_GetAdapterModeCount,
    (void *)d3dadapter9_EnumAdapterModes,
    (void *)d3dadapter9_GetAdapterDisplayMode,
    (void *)d3dadapter9_CheckDeviceType,
    (void *)d3dadapter9_CheckDeviceFormat,
    (void *)d3dadapter9_CheckDeviceMultiSampleType,
    (void *)d3dadapter9_CheckDepthStencilMatch,
    (void *)d3dadapter9_CheckDeviceFormatConversion,
    (void *)d3dadapter9_GetDeviceCaps,
    (void *)d3dadapter9_GetAdapterMonitor,
    (void *)d3dadapter9_CreateDevice,
    (void *)d3dadapter9_GetAdapterModeCountEx,
    (void *)d3dadapter9_EnumAdapterModesEx,
    (void *)d3dadapter9_GetAdapterDisplayModeEx,
    (void *)d3dadapter9_CreateDeviceEx,
    (void *)d3dadapter9_GetAdapterLUID
};

HRESULT
d3dadapter9_new( BOOL ex, Display *dpy,
                 IDirect3D9Ex **ppOut )
{
    static void * WINAPI (*pD3DAdapter9GetProc)(const char *) = NULL;
    static BOOL StaticInitDone = FALSE;

    // load dynamic library and retrieve "D3DAdapter9GetProc" symbol.
    if (!StaticInitDone) {
        StaticInitDone = TRUE;

        if (!PRESENTCheckExtension(dpy, 1, 0)) {
            ERR("Unable to query PRESENT.\n");
            return D3DERR_NOTAVAILABLE;
        }

        if (!DRI3CheckExtension(dpy, 1, 0)) {
            ERR("Unable to query DRI3.\n");
            return D3DERR_NOTAVAILABLE;
        }


        void * handle = NULL;

        const char *path = getenv("D3D_MODULE_PATH");
        if (path) {
           /* extremely basic path parsing attempt */
           const char *dot = strrchr(path, '.');
           if (dot) {
              if (!strcmp(dot, ".so")) {
                 handle = dlopen(path, RTLD_LOCAL | RTLD_NOW);
                 if (!handle)
                    ERR("Failed to load d3d9 lib '%s': %s\n", path, dlerror());
              }
           } else {
              char str[4096];
              snprintf(str, sizeof(str), "%s/d3dadapter9.so", path);
              handle = dlopen(str, RTLD_LOCAL | RTLD_NOW);
                 ERR("Failed to load d3d9 lib '%s': %s\n", str, dlerror());
           }
        } else {
            const char *paths[] = {
                "/usr/lib32/d3d/d3dadapter9.so",
                "/usr/lib/d3d/d3dadapter9.so",
                "/usr/local/lib/x86_64-linux-gnu/d3d/d3dadapter9.so",
                "/usr/lib/x86_64-linux-gnu/d3d/d3dadapter9.so",
                "/usr/local/lib/arm-linux-gnueabihf/d3d/d3dadapter9.so",
                "/usr/lib/arm-linux-gnueabihf/d3d/d3dadapter9.so",
                "/usr/local/lib/aarch64-linux-gnu/d3d/d3dadapter9.so",
                "/usr/lib/aarch64-linux-gnu/d3d/d3dadapter9.so",
            };
            for (unsigned i = 0; !handle && i < 8; i++)
                handle = dlopen(paths[i], RTLD_LOCAL | RTLD_NOW);
            if (!handle)
                ERR("Failed to load d3d9 lib: %s\n", dlerror());
        }
        if (!handle) {
            return D3DERR_NOTAVAILABLE;
        }

        pD3DAdapter9GetProc = dlsym(handle, "D3DAdapter9GetProc");
        if (!pD3DAdapter9GetProc) {
            ERR("Failed to load d3d9 lib symbols\n");
            return D3DERR_NOTAVAILABLE;
        }
    }

    int fd;
    if (!DRI3Open(dpy, DefaultScreen(dpy), &fd)) {
        ERR("DRI3Open failed (fd=%d)\n", fd);
        return D3DERR_NOTAVAILABLE;
    }

    const struct D3DAdapter9DRM *d3d9_drm = pD3DAdapter9GetProc(D3DADAPTER9DRM_NAME);
    if (!d3d9_drm || d3d9_drm->major_version != D3DADAPTER9DRM_MAJOR)
    {
        ERR("Your display driver doesn't support native D3D9 adapters.\n");
        return D3DERR_NOTAVAILABLE;
    }

    ID3DAdapter9* adapter = NULL;
    HRESULT hr = d3d9_drm->create_adapter(fd, &adapter);
    if (FAILED(hr)) {
        ERR("Unable to create ID3DAdapter9 (fd=%d)\n", fd);
        return hr;
    }

    struct d3dadapter9 *This = calloc(1, sizeof(struct d3dadapter9));
    if (!This) {
        ERR("Out of memory.\n");
        return E_OUTOFMEMORY;
    }

    This->vtable = &d3dadapter9_vtable;
    This->refs = 1;
    This->ex = ex;
    This->adapter = adapter;

    *ppOut = (IDirect3D9Ex *)This;
    FIXME("\033[1;32m\nNative Direct3D 9 is active.\033[0m\n");
    return D3D_OK;
}


// --------------------------------------------------------------------

static IDirect3D9Ex* Direct3DCreate9Ex_SDL_common(BOOL ex, SDL_Window *win )
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_bool Ok = SDL_GetWindowWMInfo(win, &info);
    if (!Ok) {
        ERR("SDL_GetWindowWMInfo failed.");
        return NULL;
    }

    IDirect3D9Ex *pD3D9Ex = NULL;
    HRESULT hr = d3dadapter9_new( ex, info.info.x11.display, &pD3D9Ex );
    if (FAILED(hr)) {
        return NULL;
    }

    return pD3D9Ex;
}

IDirect3D9Ex* SDL_Direct3DCreate9Ex(SDL_Window *win)
{
    return Direct3DCreate9Ex_SDL_common(TRUE, win);
}


IDirect3D9* Direct3DCreate9_SDL(SDL_Window *win)
{
    return (IDirect3D9*)Direct3DCreate9Ex_SDL_common(FALSE, win);
}
