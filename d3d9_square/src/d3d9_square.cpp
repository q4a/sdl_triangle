
#include <d3d9.h>
#include <string.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#define ok(c, ...) {{if (!(c)) fprintf(stdout, "fail %s ", __func__); else fprintf(stdout, "succ %s ", __func__); fprintf(stdout, __VA_ARGS__);}}
#define skip(...) {fprintf(stdout, "skip "); fprintf(stdout, __VA_ARGS__);}
#define win_skip(...) {fprintf(stdout, "skip "); fprintf(stdout, __VA_ARGS__);}
#define trace(...) {fprintf(stdout, "trace ");fprintf(stdout, __VA_ARGS__);}

struct vec3
{
	float x, y, z;
};

struct vec4
{
	float x, y, z, w;
};

static BOOL compare_uint(unsigned int x, unsigned int y, unsigned int max_diff)
{
	unsigned int diff = x > y ? x - y : y - x;

	return diff <= max_diff;
}

static BOOL color_match(D3DCOLOR c1, D3DCOLOR c2, BYTE max_diff)
{
	return compare_uint(c1 & 0xff, c2 & 0xff, max_diff)
		&& compare_uint((c1 >> 8) & 0xff, (c2 >> 8) & 0xff, max_diff)
		&& compare_uint((c1 >> 16) & 0xff, (c2 >> 16) & 0xff, max_diff)
		&& compare_uint((c1 >> 24) & 0xff, (c2 >> 24) & 0xff, max_diff);
}

static HWND create_window(void)
{
	HWND hwnd;
	RECT rect;

	SetRect(&rect, 0, 0, 640, 480);
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);
	hwnd = CreateWindowA("static", "d3d9_test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		0, 0, rect.right - rect.left, rect.bottom - rect.top, 0, 0, 0, 0);
	return hwnd;
}

struct surface_readback
{
	IDirect3DSurface9* surface;
	D3DLOCKED_RECT locked_rect;
};

static void get_rt_readback(IDirect3DSurface9* surface, struct surface_readback* rb)
{
	IDirect3DDevice9* device;
	D3DSURFACE_DESC desc;
	HRESULT hr;

	memset(rb, 0, sizeof(*rb));
	hr = IDirect3DSurface9_GetDevice(surface, &device);
	ok(SUCCEEDED(hr), "Failed to get device, hr %#x.\n", hr);
	hr = IDirect3DSurface9_GetDesc(surface, &desc);
	ok(SUCCEEDED(hr), "Failed to get surface desc, hr %#x.\n", hr);
	hr = IDirect3DDevice9_CreateOffscreenPlainSurface(device, desc.Width, desc.Height,
		desc.Format, D3DPOOL_SYSTEMMEM, &rb->surface, NULL);
	if (FAILED(hr) || !rb->surface)
	{
		trace("Can't create an offscreen plain surface to read the render target data, hr %#x.\n", hr);
		goto error;
	}

	hr = IDirect3DDevice9_GetRenderTargetData(device, surface, rb->surface);
	if (FAILED(hr))
	{
		trace("Can't read the render target data, hr %#x.\n", hr);
		goto error;
	}

	hr = IDirect3DSurface9_LockRect(rb->surface, &rb->locked_rect, NULL, D3DLOCK_READONLY);
	if (FAILED(hr))
	{
		trace("Can't lock the offscreen surface, hr %#x.\n", hr);
		goto error;
	}
	IDirect3DDevice9_Release(device);

	return;

error:
	if (rb->surface)
		IDirect3DSurface9_Release(rb->surface);
	rb->surface = NULL;
	IDirect3DDevice9_Release(device);
}

static DWORD get_readback_color(struct surface_readback* rb, unsigned int x, unsigned int y)
{
	return rb->locked_rect.pBits
		? ((DWORD*)rb->locked_rect.pBits)[y * rb->locked_rect.Pitch / sizeof(DWORD) + x] : 0xdeadbeef;
}

static void release_surface_readback(struct surface_readback* rb)
{
	HRESULT hr;

	if (!rb->surface)
		return;
	if (rb->locked_rect.pBits && FAILED(hr = IDirect3DSurface9_UnlockRect(rb->surface)))
		trace("Can't unlock the offscreen surface, hr %#x.\n", hr);
	IDirect3DSurface9_Release(rb->surface);
}

static DWORD getPixelColor(IDirect3DDevice9* device, UINT x, UINT y)
{
	DWORD ret;
	IDirect3DSurface9* rt;
	struct surface_readback rb;
	HRESULT hr;

	hr = IDirect3DDevice9_GetRenderTarget(device, 0, &rt);
	if (FAILED(hr))
	{
		trace("Can't get the render target, hr %#x.\n", hr);
		return 0xdeadbeed;
	}

	get_rt_readback(rt, &rb);
	/* Remove the X channel for now. DirectX and OpenGL have different ideas how to treat it apparently, and it isn't
	 * really important for these tests
	 */
	ret = get_readback_color(&rb, x, y) & 0x00ffffff;
	release_surface_readback(&rb);

	IDirect3DSurface9_Release(rt);
	return ret;
}

static IDirect3DDevice9* create_device(IDirect3D9* d3d, HWND device_window, HWND focus_window, BOOL windowed)
{
	D3DPRESENT_PARAMETERS present_parameters = { 0 };
	IDirect3DDevice9* device;

	present_parameters.Windowed = windowed;
	present_parameters.hDeviceWindow = device_window;
	present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	present_parameters.BackBufferWidth = 640;
	present_parameters.BackBufferHeight = 480;
	present_parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
	present_parameters.EnableAutoDepthStencil = TRUE;
	present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;

	if (SUCCEEDED(IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, focus_window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device)))
		return device;

	return NULL;
}

// main ... The main function, right now it just calls the initialization of SDL.
int main(int argc, char* argv[]) {
	
	IDirect3DVertexShader9* vertex_shader[2] = { NULL, NULL, };
	IDirect3DPixelShader9* pixel_shader[3] = { NULL, NULL, NULL, };
	float start = 0.0f, end = 1.5f;
	HRESULT hr;
	IDirect3DDevice9* device;
	IDirect3D9* d3d;
	ULONG refcount;
	HWND window;
	D3DCOLOR color;
	D3DCAPS9 caps;
	/* basic vertex shader with reversed fog computation ("foggy") */
	static const DWORD vertex_shader_code1[] =
	{
		0xfffe0101,                                                             /* vs_1_1                        */
		0x0000001f, 0x80000000, 0x900f0000,                                     /* dcl_position v0               */
		0x0000001f, 0x8000000a, 0x900f0001,                                     /* dcl_color0 v1                 */
		0x00000051, 0xa00f0000, 0xbfa00000, 0x00000000, 0xbf666666, 0x00000000, /* def c0, -1.25, 0.0, -0.9, 0.0 */
		0x00000001, 0xc00f0000, 0x90e40000,                                     /* mov oPos, v0                  */
		0x00000001, 0xd00f0000, 0x90e40001,                                     /* mov oD0, v1                   */
		0x00000002, 0x800f0000, 0x90aa0000, 0xa0aa0000,                         /* add r0, v0.z, c0.z            */
		0x00000005, 0xc00f0001, 0x80000000, 0xa0000000,                         /* mul oFog, r0.x, c0.x          */
		0x0000ffff
	};
	/* basic pixel shader */
	static const DWORD pixel_shader_code1[] =
	{
		0xffff0101,                                                             /* ps_1_1     */
		0x00000001, 0x800f0000, 0x90e40000,                                     /* mov r0, v0 */
		0x0000ffff
	};
	static const DWORD pixel_shader_code2[] =
	{
		0xffff0200,                                                             /* ps_2_0                     */
		0x05000051, 0xa00f0000, 0x3f000000, 0x00000000, 0x00000000, 0x00000000, /* def c0, 0.5, 0.0, 0.0, 0.0 */
		0x0200001f, 0x80000000, 0x900f0000,                                     /* dcl v0                     */
		0x02000001, 0x800f0800, 0x90e40000,                                     /* mov oC0, v0                */
//		0x02000001, 0x900f0800, 0xa0000000,                                     /* mov oDepth, c0.x           */
		0x02000001, 0x80080000, 0xa0000000,                                     /* mov r0.w, c0.x             */
		0x02000001, 0x900f0800, 0x80ff0000,                                     /* mov oDepth, r0.w           */
		0x0000ffff
	};
	static struct
	{
		struct vec4 position;
		D3DCOLOR diffuse;
	}
	untransformed_q[] =
	{
		{ {160.0f, 120.0f, 0.0f, 0.0f}, 0xffff0000},
		{ {480.0f, 120.0f, 0.0f, 0.0f}, 0xffff0000},
		{ {160.0f, 360.0f, 0.0f, 0.0f}, 0xffff0000},
		{ {480.0f, 360.0f, 0.0f, 0.0f}, 0xffff0000},
	};
	static struct
	{
		struct vec3 position;
		D3DCOLOR diffuse;
	}
	transformed_q[] =
	{
		{{-0.5f,  0.5f, 0.0f}, 0xffff0000},
		{{ 0.5f,  0.5f, 0.0f}, 0xffff0000},
		{{-0.5f, -0.5f, 0.0f}, 0xffff0000},
		{{ 0.5f, -0.5f, 0.0f}, 0xffff0000},
	};

	// projection matrix.
	static const D3DMATRIX proj[] =
	{
	{{{
		0.75f, 0.0f,  0.0f,   0.0f,
		0.0f,  1.0f,  0.0f,   0.0f,
		0.0f,  0.0f,  0.202f, 1.0f,
		0.0f,  0.0f, -0.202f, 0.0f
	}}},
	{{{
		1.5f,  0.0f,  0.0f,   0.0f,
		0.0f,  2.0f,  0.0f,   0.0f,
		0.0f,  0.0f,  0.404f, 2.0f,
		0.0f,  0.0f, -0.404f, 0.0f
	}}},
	{{{
		1.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  1.0f,  0.0f,  0.0f,
		0.0f,  0.0f,  1.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  1.0f
	}}},
	};
	static const struct
	{
		int vshader, pshader;
		unsigned int matrix_id;
		float z, rhw;
		unsigned int format_bits;
		D3DCOLOR color1, color2;
	}
	tests[] =
	{
		//0-9
		{0, 0, 0, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{0, 0, 0, 0.2f, 0.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{0, 0, 0, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{0, 0, 0, 1.2f, 1.2f, D3DFVF_XYZ,    0x0008f600, 0x0008f600},
		{0, 0, 1, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{0, 0, 1, 0.2f, 0.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{0, 0, 1, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{0, 0, 1, 1.2f, 1.2f, D3DFVF_XYZ,    0x0000ff00, 0x0000ff00},
		{0, 0, 2, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x00b24c00, 0x00b24c00},
		{0, 0, 2, 0.2f, 0.2f, D3DFVF_XYZ,    0x00b24c00, 0x00b24c00},
		//10-19
		{0, 0, 2, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x0008f600, 0x0008f600},
		{0, 0, 2, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{0, 1, 0, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{0, 1, 0, 0.2f, 0.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{0, 1, 0, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{0, 1, 0, 1.2f, 1.2f, D3DFVF_XYZ,    0x0008f600, 0x0008f600},
		{0, 1, 1, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{0, 1, 1, 0.2f, 0.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{0, 1, 1, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{0, 1, 1, 1.2f, 1.2f, D3DFVF_XYZ,    0x0000ff00, 0x0000ff00},
		//20-29
		{0, 1, 2, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x00b24c00, 0x00b24c00},
		{0, 1, 2, 0.2f, 0.2f, D3DFVF_XYZ,    0x00b24c00, 0x00b24c00},
		{0, 1, 2, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x0008f600, 0x0008f600},
		{0, 1, 2, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{1, 0, 0, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{1, 0, 0, 0.2f, 0.2f, D3DFVF_XYZ,    0x0055aa00, 0x0055aa00},//has different color compared to version without ps/vs
		{1, 0, 0, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{1, 0, 0, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},//has different color compared to version without ps/vs
		{1, 0, 1, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{1, 0, 1, 0.2f, 0.2f, D3DFVF_XYZ,    0x0055aa00, 0x0055aa00},//has different color compared to version without ps/vs
		//30-39
		{1, 0, 1, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{1, 0, 1, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},//has different color compared to version without ps/vs
		{1, 0, 2, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x00b24c00, 0x00b24c00},
		{1, 0, 2, 0.2f, 0.2f, D3DFVF_XYZ,    0x00b24c00, 0x00b24c00},
		{1, 0, 2, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x0008f600, 0x0008f600},
		{1, 0, 2, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{1, 1, 0, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{1, 1, 0, 0.2f, 0.2f, D3DFVF_XYZ,    0x0055aa00, 0x0055aa00},//has different color compared to version without ps/vs
		{1, 1, 0, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{1, 1, 0, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},//has different color compared to version without ps/vs
		//40-49
		{1, 1, 1, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{1, 1, 1, 0.2f, 0.2f, D3DFVF_XYZ,    0x0055aa00, 0x0055aa00},//has different color compared to version without ps/vs
		{1, 1, 1, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{1, 1, 1, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},//has different color compared to version without ps/vs
		{1, 1, 2, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x00b24c00, 0x00b24c00},
		{1, 1, 2, 0.2f, 0.2f, D3DFVF_XYZ,    0x00b24c00, 0x00b24c00},
		{1, 1, 2, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x0008f600, 0x0008f600},
		{1, 1, 2, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{0, 2, 0, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{0, 2, 0, 0.2f, 0.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		//50-59
		{0, 2, 0, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{0, 2, 0, 1.2f, 1.2f, D3DFVF_XYZ,    0x0008f600, 0x0008f600},
		{0, 2, 1, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{0, 2, 1, 0.2f, 0.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{0, 2, 1, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{0, 2, 1, 1.2f, 1.2f, D3DFVF_XYZ,    0x0000ff00, 0x0000ff00},
		{0, 2, 2, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x00b24c00, 0x00aa5500},//Radeon HD8400-left, Ivy Bridge GT1-right
		{0, 2, 2, 0.2f, 0.2f, D3DFVF_XYZ,    0x00b24c00, 0x00aa5500},//Radeon HD8400-left, Ivy Bridge GT1-right
		{0, 2, 2, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x0008f600, 0x00aa5500},//Radeon HD8400-left, Ivy Bridge GT1-right
		{0, 2, 2, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		//60-69
		{1, 2, 0, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{1, 2, 0, 0.2f, 0.2f, D3DFVF_XYZ,    0x0055aa00, 0x0055aa00},
		{1, 2, 0, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{1, 2, 0, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{1, 2, 1, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x0000ff00, 0x0000ff00},
		{1, 2, 1, 0.2f, 0.2f, D3DFVF_XYZ,    0x0055aa00, 0x0055aa00},
		{1, 2, 1, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x00986700, 0x00986700},
		{1, 2, 1, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
		{1, 2, 2, 0.2f, 0.2f, D3DFVF_XYZRHW, 0x00b24c00, 0x00aa5500},//Radeon HD8400-left, Ivy Bridge GT1-right
		{1, 2, 2, 0.2f, 0.2f, D3DFVF_XYZ,    0x00b24c00, 0x00aa5500},//Radeon HD8400-left, Ivy Bridge GT1-right
		//70-71
		{1, 2, 2, 1.2f, 1.2f, D3DFVF_XYZRHW, 0x0008f600, 0x00aa5500},//Radeon HD8400-left, Ivy Bridge GT1-right
		{1, 2, 2, 1.2f, 1.2f, D3DFVF_XYZ,    0x000000ff, 0x000000ff},
	};
	unsigned int i;

	window = create_window();
	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	ok(!!d3d, "Failed to create a D3D object.\n");

	if (!(device = create_device(d3d, window, window, TRUE)))
	{
		skip("Failed to create a D3D device, skipping tests.\n");
		IDirect3D9_Release(d3d);
		DestroyWindow(window);
		return 1;
	}
	
	hr = IDirect3DDevice9_GetDeviceCaps(device, &caps);
	ok(SUCCEEDED(hr), "Failed to get device caps, hr %#x.\n", hr);
	if (!(caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE))
	{
		skip("D3DPRASTERCAPS_FOGTABLE not supported, skipping POSITIONT table fog test.\n");
		goto done;
	}

	hr = IDirect3DDevice9_CreateVertexShader(device, vertex_shader_code1, &vertex_shader[1]);
	ok(SUCCEEDED(hr), "CreateVertexShader failed (%08x)\n", hr);
	hr = IDirect3DDevice9_CreatePixelShader(device, pixel_shader_code1, &pixel_shader[1]);
	ok(SUCCEEDED(hr), "CreatePixelShader failed (%08x)\n", hr);
	hr = IDirect3DDevice9_CreatePixelShader(device, pixel_shader_code2, &pixel_shader[2]);
	ok(SUCCEEDED(hr), "CreatePixelShader failed (%08x)\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_LIGHTING, FALSE);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGENABLE, TRUE);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGCOLOR, 0x0000ff00);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGSTART, *(DWORD*)(&start));
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGEND, *(DWORD*)(&end));
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_CLIPPING, FALSE);
	ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, D3DZB_FALSE);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);

	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));
	static float lastTime = (float)timeGetTime();
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			float currTime = (float)timeGetTime();
			float timeDelta = (currTime - lastTime) * 0.001f;

	
	for (i = 0; i < ARRAY_SIZE(tests); ++i)
	//i = 4;
	{
		hr = IDirect3DDevice9_SetTransform(device, D3DTS_PROJECTION, &proj[tests[i].matrix_id]);
		ok(SUCCEEDED(hr), "Failed to set projection transform, hr %#x.\n", hr);
		hr = IDirect3DDevice9_SetFVF(device, tests[i].format_bits | D3DFVF_DIFFUSE);
		ok(SUCCEEDED(hr), "Failed to set fvf, hr %#x.\n", hr);
		hr = IDirect3DDevice9_SetVertexShader(device, vertex_shader[tests[i].vshader]);
		ok(SUCCEEDED(hr), "SetVertexShader failed (%08x)\n", hr);
		hr = IDirect3DDevice9_SetPixelShader(device, pixel_shader[tests[i].pshader]);
		ok(SUCCEEDED(hr), "SetPixelShader failed (%08x)\n", hr);
		hr = IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x000000ff, 1.0f, 0);
		ok(SUCCEEDED(hr), "Failed to clear, hr %#x.\n", hr);

		if (tests[i].format_bits == D3DFVF_XYZRHW)
		{
			untransformed_q[0].position.z = 0.1f + tests[i].z;
			untransformed_q[1].position.z = 0.2f + tests[i].z;
			untransformed_q[2].position.z = 0.3f + tests[i].z;
			untransformed_q[3].position.z = 0.4f + tests[i].z;
			untransformed_q[0].position.w = 0.6f + tests[i].rhw;
			untransformed_q[1].position.w = 0.5f + tests[i].rhw;
			untransformed_q[2].position.w = 0.4f + tests[i].rhw;
			untransformed_q[3].position.w = 0.3f + tests[i].rhw;
			hr = IDirect3DDevice9_BeginScene(device);
			ok(SUCCEEDED(hr), "Failed to begin scene, hr %#x.\n", hr);
			hr = IDirect3DDevice9_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, untransformed_q, sizeof(untransformed_q[0]));
			ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);
			hr = IDirect3DDevice9_EndScene(device);
			ok(SUCCEEDED(hr), "Failed to end scene, hr %#x.\n", hr);
		}
		else
		{
			transformed_q[0].position.z = 0.1f + tests[i].z;
			transformed_q[1].position.z = 0.2f + tests[i].z;
			transformed_q[2].position.z = 0.3f + tests[i].z;
			transformed_q[3].position.z = 0.4f + tests[i].z;
			hr = IDirect3DDevice9_BeginScene(device);
			ok(SUCCEEDED(hr), "Failed to begin scene, hr %#x.\n", hr);
			hr = IDirect3DDevice9_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, transformed_q, sizeof(transformed_q[0]));
			ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);
			hr = IDirect3DDevice9_EndScene(device);
			ok(SUCCEEDED(hr), "Failed to end scene, hr %#x.\n", hr);
		}

		color = getPixelColor(device, 320, 240);
		ok(color_match(color, tests[i].color1, 2) || color_match(color, tests[i].color2, 2),
			"Got unexpected color 0x%08x, expected 0x%08x or 0x%08x, case %u.\n", color, tests[i].color1, tests[i].color2, i);
		hr = IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL);
		ok(SUCCEEDED(hr), "Failed to present, hr %#x.\n", hr);
	}

			lastTime = currTime;
		}
	}

	IDirect3DVertexShader9_Release(vertex_shader[1]);
	IDirect3DPixelShader9_Release(pixel_shader[1]);
	IDirect3DPixelShader9_Release(pixel_shader[2]);
done:
	refcount = IDirect3DDevice9_Release(device);
	ok(!refcount, "Device has %u references left.\n", refcount);
	IDirect3D9_Release(d3d);
	DestroyWindow(window);
	return 0;
}
