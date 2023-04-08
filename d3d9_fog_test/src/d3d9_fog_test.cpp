
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

	float start = 0.0f, end = 1.0f;
	IDirect3DDevice9* device;
	IDirect3D9* d3d;
	D3DCOLOR color;
	ULONG refcount;
	D3DCAPS9 caps;
	HWND window;
	HRESULT hr;
	int i;

	/* Gets full z based fog with linear fog, no fog with specular color. */
	static const struct
	{
		float x, y, z;
		D3DCOLOR diffuse;
		D3DCOLOR specular;
	}
	untransformed_1[] =
	{
		{-1.0f, -1.0f, 0.1f, 0xffff0000, 0xff000000},
		{-1.0f,  0.0f, 0.1f, 0xffff0000, 0xff000000},
		{ 0.0f,  0.0f, 0.1f, 0xffff0000, 0xff000000},
		{ 0.0f, -1.0f, 0.1f, 0xffff0000, 0xff000000},
	},
	/* Ok, I am too lazy to deal with transform matrices. */
	untransformed_2[] =
	{
		{-1.0f,  0.0f, 1.0f, 0xffff0000, 0xff000000},
		{-1.0f,  1.0f, 1.0f, 0xffff0000, 0xff000000},
		{ 0.0f,  1.0f, 1.0f, 0xffff0000, 0xff000000},
		{ 0.0f,  0.0f, 1.0f, 0xffff0000, 0xff000000},
	},
	untransformed_3[] =
	{
		{-1.0f, -1.0f, 0.5f, 0xffff0000, 0xff000000},
		{-1.0f,  1.0f, 0.5f, 0xffff0000, 0xff000000},
		{ 1.0f, -1.0f, 0.5f, 0xffff0000, 0xff000000},
		{ 1.0f,  1.0f, 0.5f, 0xffff0000, 0xff000000},
	},
	far_quad1[] =
	{
		{-1.0f, -1.0f, 0.5f, 0xffff0000, 0xff000000},
		{-1.0f,  0.0f, 0.5f, 0xffff0000, 0xff000000},
		{ 0.0f,  0.0f, 0.5f, 0xffff0000, 0xff000000},
		{ 0.0f, -1.0f, 0.5f, 0xffff0000, 0xff000000},
	},
	far_quad2[] =
	{
		{-1.0f,  0.0f, 1.5f, 0xffff0000, 0xff000000},
		{-1.0f,  1.0f, 1.5f, 0xffff0000, 0xff000000},
		{ 0.0f,  1.0f, 1.5f, 0xffff0000, 0xff000000},
		{ 0.0f,  0.0f, 1.5f, 0xffff0000, 0xff000000},
	};
	/* Untransformed ones. Give them a different diffuse color to make the
	 * test look nicer. It also makes making sure that they are drawn
	 * correctly easier. */
	static const struct
	{
		float x, y, z, rhw;
		D3DCOLOR diffuse;
		D3DCOLOR specular;
	}
	transformed_1[] =
	{
		{320.0f,   0.0f, 1.0f, 1.0f, 0xffffff00, 0xff000000},
		{640.0f,   0.0f, 1.0f, 1.0f, 0xffffff00, 0xff000000},
		{640.0f, 240.0f, 1.0f, 1.0f, 0xffffff00, 0xff000000},
		{320.0f, 240.0f, 1.0f, 1.0f, 0xffffff00, 0xff000000},
	},
	transformed_2[] =
	{
		{320.0f, 240.0f, 1.0f, 1.0f, 0xffffff00, 0xff000000},
		{640.0f, 240.0f, 1.0f, 1.0f, 0xffffff00, 0xff000000},
		{640.0f, 480.0f, 1.0f, 1.0f, 0xffffff00, 0xff000000},
		{320.0f, 480.0f, 1.0f, 1.0f, 0xffffff00, 0xff000000},
	};
	static const struct
	{
		struct vec3 position;
		DWORD diffuse;
	}
	rev_fog_quads[] =
	{
		{{-1.0f, -1.0f, 0.1f}, 0x000000ff},
		{{-1.0f,  0.0f, 0.1f}, 0x000000ff},
		{{ 0.0f,  0.0f, 0.1f}, 0x000000ff},
		{{ 0.0f, -1.0f, 0.1f}, 0x000000ff},

		{{ 0.0f, -1.0f, 0.9f}, 0x000000ff},
		{{ 0.0f,  0.0f, 0.9f}, 0x000000ff},
		{{ 1.0f,  0.0f, 0.9f}, 0x000000ff},
		{{ 1.0f, -1.0f, 0.9f}, 0x000000ff},

		{{ 0.0f,  0.0f, 0.4f}, 0x000000ff},
		{{ 0.0f,  1.0f, 0.4f}, 0x000000ff},
		{{ 1.0f,  1.0f, 0.4f}, 0x000000ff},
		{{ 1.0f,  0.0f, 0.4f}, 0x000000ff},

		{{-1.0f,  0.0f, 0.7f}, 0x000000ff},
		{{-1.0f,  1.0f, 0.7f}, 0x000000ff},
		{{ 0.0f,  1.0f, 0.7f}, 0x000000ff},
		{{ 0.0f,  0.0f, 0.7f}, 0x000000ff},
	};
	static const D3DMATRIX ident_mat =
	{ {{
		1.0f, 0.0f,  0.0f, 0.0f,
		0.0f, 1.0f,  0.0f, 0.0f,
		0.0f, 0.0f,  1.0f, 0.0f,
		0.0f, 0.0f,  0.0f, 1.0f
	}} };
	static const D3DMATRIX world_mat1 =
	{ {{
		1.0f, 0.0f,  0.0f, 0.0f,
		0.0f, 1.0f,  0.0f, 0.0f,
		0.0f, 0.0f,  1.0f, 0.0f,
		0.0f, 0.0f, -0.5f, 1.0f
	}} };
	static const D3DMATRIX world_mat2 =
	{ {{
		1.0f, 0.0f,  0.0f, 0.0f,
		0.0f, 1.0f,  0.0f, 0.0f,
		0.0f, 0.0f,  1.0f, 0.0f,
		0.0f, 0.0f,  1.0f, 1.0f
	}} };
	static const D3DMATRIX proj_mat =
	{ {{
		1.0f, 0.0f,  0.0f, 0.0f,
		0.0f, 1.0f,  0.0f, 0.0f,
		0.0f, 0.0f,  1.0f, 0.0f,
		0.0f, 0.0f, -1.0f, 1.0f
	}} };
	static const WORD Indices[] = { 0, 1, 2, 2, 3, 0 };
	static const WORD Indices2[] =
	{
		 0,  1,  2,  2,  3,  0,
		 4,  5,  6,  6,  7,  4,
		 8,  9, 10, 10, 11,  8,
		12, 13, 14, 14, 15, 12,
	};

	window = create_window();
	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	ok(!!d3d, "Failed to create a D3D object.\n");
	if (!(device = create_device(d3d, window, window, TRUE)))
	{
		skip("Failed to create a D3D device, skipping tests.\n");
		goto done;
	}

	memset(&caps, 0, sizeof(caps));
	hr = IDirect3DDevice9_GetDeviceCaps(device, &caps);
	ok(hr == D3D_OK, "IDirect3DDevice9_GetDeviceCaps returned %08x\n", hr);
	hr = IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffff00ff, 0.0, 0);
	ok(hr == D3D_OK, "IDirect3DDevice9_Clear returned %08x\n", hr);

	/* Setup initial states: No lighting, fog on, fog color */
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, FALSE);
	ok(SUCCEEDED(hr), "Failed to disable D3DRS_ZENABLE, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_LIGHTING, FALSE);
	ok(hr == D3D_OK, "Turning off lighting returned %08x\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGENABLE, TRUE);
	ok(hr == D3D_OK, "Turning on fog calculations returned %08x\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGCOLOR, 0xff00ff00 /* A nice green */);
	ok(hr == D3D_OK, "Setting fog color returned %#08x\n", hr);
	/* Some of the tests seem to depend on the projection matrix explicitly
	 * being set to an identity matrix, even though that's the default.
	 * (AMD Radeon HD 6310, Windows 7) */
	hr = IDirect3DDevice9_SetTransform(device, D3DTS_PROJECTION, &ident_mat);
	ok(SUCCEEDED(hr), "Failed to set projection transform, hr %#x.\n", hr);

	/* First test: Both table fog and vertex fog off */
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGTABLEMODE, D3DFOG_NONE);
	ok(hr == D3D_OK, "Turning off table fog returned %08x\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
	ok(hr == D3D_OK, "Turning off vertex fog returned %08x\n", hr);

	/* Start = 0, end = 1. Should be default, but set them */
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGSTART, *((DWORD*)&start));
	ok(hr == D3D_OK, "Setting fog start returned %08x\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGEND, *((DWORD*)&end));
	ok(hr == D3D_OK, "Setting fog end returned %08x\n", hr);

	hr = IDirect3DDevice9_BeginScene(device);
	ok(SUCCEEDED(hr), "Failed to begin scene, hr %#x.\n", hr);

	hr = IDirect3DDevice9_SetFVF(device, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
	ok(SUCCEEDED(hr), "Failed to set FVF, hr %#x.\n", hr);

	/* Untransformed, vertex fog = NONE, table fog = NONE:
	 * Read the fog weighting from the specular color. */
	hr = IDirect3DDevice9_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
		2 /* PrimCount */, Indices, D3DFMT_INDEX16, untransformed_1, sizeof(untransformed_1[0]));
	ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);

	/* That makes it use the Z value */
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
	ok(SUCCEEDED(hr), "Failed to set D3DFOG_LINEAR fog vertex mode, hr %#x.\n", hr);
	/* Untransformed, vertex fog != none (or table fog != none):
	 * Use the Z value as input into the equation. */
	hr = IDirect3DDevice9_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
		2 /* PrimCount */, Indices, D3DFMT_INDEX16, untransformed_2, sizeof(untransformed_2[0]));
	ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);

	/* transformed verts */
	hr = IDirect3DDevice9_SetFVF(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
	ok(SUCCEEDED(hr), "Failed to set FVF, hr %#x.\n", hr);
	/* Transformed, vertex fog != NONE, pixel fog == NONE:
	 * Use specular color alpha component. */
	hr = IDirect3DDevice9_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
		2 /* PrimCount */, Indices, D3DFMT_INDEX16, transformed_1, sizeof(transformed_1[0]));
	ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);

	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	ok(SUCCEEDED(hr), "Failed to set D3DFOG_LINEAR fog table mode, hr %#x.\n", hr);
	/* Transformed, table fog != none, vertex anything:
	 * Use Z value as input to the fog equation. */
	hr = IDirect3DDevice9_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
		2 /* PrimCount */, Indices, D3DFMT_INDEX16, transformed_2, sizeof(transformed_2[0]));
	ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);

	hr = IDirect3DDevice9_EndScene(device);
	ok(hr == D3D_OK, "EndScene returned %08x\n", hr);

	color = getPixelColor(device, 160, 360);
	ok(color == 0x00ff0000, "Untransformed vertex with no table or vertex fog has color %08x\n", color);
	color = getPixelColor(device, 160, 120);
	ok(color_match(color, 0x0000ff00, 1), "Untransformed vertex with linear vertex fog has color %08x\n", color);
	color = getPixelColor(device, 480, 120);
	ok(color == 0x00ffff00, "Transformed vertex with linear vertex fog has color %08x\n", color);
	if (caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE)
	{
		color = getPixelColor(device, 480, 360);
		ok(color_match(color, 0x0000ff00, 1), "Transformed vertex with linear table fog has color %08x\n", color);
	}
	else
	{
		/* Without fog table support the vertex fog is still applied, even though table fog is turned on.
		 * The settings above result in no fogging with vertex fog
		 */
		color = getPixelColor(device, 480, 120);
		ok(color == 0x00ffff00, "Transformed vertex with linear vertex fog has color %08x\n", color);
		trace("Info: Table fog not supported by this device\n");
	}
	IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL);

	/* Now test the special case fogstart == fogend */
	hr = IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xff0000ff, 0.0, 0);
	ok(hr == D3D_OK, "IDirect3DDevice9_Clear returned %08x\n", hr);

	hr = IDirect3DDevice9_BeginScene(device);
	ok(SUCCEEDED(hr), "Failed to begin scene, hr %#x.\n", hr);

	start = 512;
	end = 512;
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGSTART, *((DWORD*)&start));
	ok(SUCCEEDED(hr), "Failed to set fog start, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGEND, *((DWORD*)&end));
	ok(SUCCEEDED(hr), "Failed to set fog end, hr %#x.\n", hr);

	hr = IDirect3DDevice9_SetFVF(device, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
	ok(SUCCEEDED(hr), "Failed to set FVF, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
	ok(SUCCEEDED(hr), "Failed to set D3DFOG_LINEAR fog vertex mode, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGTABLEMODE, D3DFOG_NONE);
	ok(SUCCEEDED(hr), "Failed to set D3DFOG_NONE fog table mode, hr %#x.\n", hr);

	/* Untransformed vertex, z coord = 0.1, fogstart = 512, fogend = 512.
	 * Would result in a completely fog-free primitive because start > zcoord,
	 * but because start == end, the primitive is fully covered by fog. The
	 * same happens to the 2nd untransformed quad with z = 1.0. The third
	 * transformed quad remains unfogged because the fogcoords are read from
	 * the specular color and has fixed fogstart and fogend. */
	hr = IDirect3DDevice9_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
		2 /* PrimCount */, Indices, D3DFMT_INDEX16, untransformed_1, sizeof(untransformed_1[0]));
	ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);
	hr = IDirect3DDevice9_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
		2 /* PrimCount */, Indices, D3DFMT_INDEX16, untransformed_2, sizeof(untransformed_2[0]));
	ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);

	hr = IDirect3DDevice9_SetFVF(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
	ok(SUCCEEDED(hr), "Failed to set FVF, hr %#x.\n", hr);
	/* Transformed, vertex fog != NONE, pixel fog == NONE:
	 * Use specular color alpha component. */
	hr = IDirect3DDevice9_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
		2 /* PrimCount */, Indices, D3DFMT_INDEX16, transformed_1, sizeof(transformed_1[0]));
	ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);

	hr = IDirect3DDevice9_EndScene(device);
	ok(SUCCEEDED(hr), "Failed to end scene, hr %#x.\n", hr);

	color = getPixelColor(device, 160, 360);
	ok(color_match(color, 0x0000ff00, 1), "Untransformed vertex with vertex fog and z = 0.1 has color %08x\n", color);
	color = getPixelColor(device, 160, 120);
	ok(color_match(color, 0x0000ff00, 1), "Untransformed vertex with vertex fog and z = 1.0 has color %08x\n", color);
	color = getPixelColor(device, 480, 120);
	ok(color == 0x00ffff00, "Transformed vertex with linear vertex fog has color %08x\n", color);
	IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL);

	/* Test "reversed" fog without shaders. With shaders this fails on a few Windows D3D implementations,
	 * but without shaders it seems to work everywhere
	 */
	end = 0.2f;
	start = 0.8f;
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGSTART, *((DWORD*)&start));
	ok(hr == D3D_OK, "Setting fog start returned %08x\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGEND, *((DWORD*)&end));
	ok(hr == D3D_OK, "Setting fog end returned %08x\n", hr);
	hr = IDirect3DDevice9_SetFVF(device, D3DFVF_XYZ | D3DFVF_DIFFUSE);
	ok(hr == D3D_OK, "IDirect3DDevice9_SetFVF returned %08x\n", hr);

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

	/* Test reversed fog without shaders. ATI cards have problems with reversed fog and shaders, so
	 * it doesn't seem very important for games. ATI cards also have problems with reversed table fog,
	 * so skip this for now
	 */
	for(i = 0; i < 2 /*2 - Table fog test disabled, fails on ATI */; i++) {
		const char *mode = (i ? "table" : "vertex");
		hr = IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffff0000, 0.0, 0);
		ok(hr == D3D_OK, "IDirect3DDevice9_Clear returned %08x\n", hr);
		hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGVERTEXMODE, i == 0 ? D3DFOG_LINEAR : D3DFOG_NONE);
		ok( hr == D3D_OK, "IDirect3DDevice9_SetRenderState returned %08x\n", hr);
		hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGTABLEMODE, i == 0 ? D3DFOG_NONE : D3DFOG_LINEAR);
		ok( hr == D3D_OK, "IDirect3DDevice9_SetRenderState returned %08x\n", hr);
		hr = IDirect3DDevice9_BeginScene(device);
		ok(SUCCEEDED(hr), "Failed to begin scene, hr %#x.\n", hr);
		hr = IDirect3DDevice9_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 16 /* NumVerts */,
				8 /* PrimCount */, Indices2, D3DFMT_INDEX16, rev_fog_quads, sizeof(rev_fog_quads[0]));
		ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);
		hr = IDirect3DDevice9_EndScene(device);
		ok(SUCCEEDED(hr), "Failed to end scene, hr %#x.\n", hr);

		color = getPixelColor(device, 160, 360);
		ok(color_match(color, 0x0000ff00, 1),
				"Reversed %s fog: z=0.1 has color 0x%08x, expected 0x0000ff00 or 0x0000fe00\n", mode, color);

		color = getPixelColor(device, 160, 120);
		ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x00, 0x2b, 0xd4), 2),
				"Reversed %s fog: z=0.7 has color 0x%08x\n", mode, color);

		color = getPixelColor(device, 480, 120);
		ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x00, 0xaa, 0x55), 2),
				"Reversed %s fog: z=0.4 has color 0x%08x\n", mode, color);

		color = getPixelColor(device, 480, 360);
		ok(color == 0x000000ff, "Reversed %s fog: z=0.9 has color 0x%08x, expected 0x000000ff\n", mode, color);

		IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL);

		if(!(caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE)) {
			skip("D3DPRASTERCAPS_FOGTABLE not supported, skipping reversed table fog test\n");
			break;
		}
	}

			lastTime = currTime;
		}
	}

done:
	IDirect3D9_Release(d3d);
	DestroyWindow(window);
	return 0;
}
