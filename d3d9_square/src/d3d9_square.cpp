
#include <d3d9.h>
#include <string.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#define ok(c, ...) {{if (!(c)) fprintf(stdout, "fail %s ", __func__); else fprintf(stdout, "succ %s ", __func__); fprintf(stdout, __VA_ARGS__);}}
#define skip(...) {fprintf(stdout, "skip "); fprintf(stdout, __VA_ARGS__);}
#define win_skip(...) {fprintf(stdout, "skip "); fprintf(stdout, __VA_ARGS__);}
#define trace(...) {fprintf(stdout, "trace ");fprintf(stdout, __VA_ARGS__);}

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
	
	HRESULT hr;
	IDirect3DDevice9* device;
	IDirect3D9* d3d;
	ULONG refcount;
	HWND window;
	D3DCOLOR color;
	D3DCAPS9 caps;
	static struct
	{
		struct vec4 position;
		D3DCOLOR diffuse;
	}
	quad[] =
	{
		{{ 40.0f,  40.0f, 0.1f, 0.6f}, 0xffff0000},
		{{600.0f,  40.0f, 0.2f, 0.7f}, 0xffff0000},
		{{ 40.0f, 440.0f, 0.3f, 0.8f}, 0xffff0000},
		{{600.0f, 440.0f, 0.4f, 0.9f}, 0xffff0000},
	};

	// projection matrix.
	// fovy:90 degrees, aspect ratio: 4/3,
	// near plane: 1, far plane: 101
	static const D3DMATRIX proj =
	{{{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}}};
	/*
	{{{
		0.75f, 0.0f,  0.0f,  0.0f,
		0.0f,  1.0f,  0.0f,  0.0f,
		0.0f,  0.0f,  1.01f, 1.0f,
		0.0f,  0.0f, -1.01f, 0.0f
	}}};
	*/
	static const struct
	{
		float z, w;
		D3DZBUFFERTYPE z_test;
		D3DCOLOR color;
	}
	tests[] =
	{
		{0.7f,  0.0f, D3DZB_TRUE,  0x004cb200},
		{0.7f,  0.0f, D3DZB_FALSE, 0x004cb200},
		{0.7f,  0.3f, D3DZB_TRUE,  0x004cb200},
		{0.7f,  0.3f, D3DZB_FALSE, 0x004cb200},
		{0.7f,  3.0f, D3DZB_TRUE,  0x004cb200},
		{0.7f,  3.0f, D3DZB_FALSE, 0x004cb200},
		{0.3f,  0.0f, D3DZB_TRUE,  0x00b24c00},
		{0.3f,  0.0f, D3DZB_FALSE, 0x00b24c00},
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

	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_LIGHTING, FALSE);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGENABLE, TRUE);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGCOLOR, 0x0000ff00);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGSTART, 0);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGEND, 1);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_CLIPPING, FALSE);
	ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetTransform(device, D3DTS_PROJECTION, &proj);
	ok(SUCCEEDED(hr), "Failed to set projection transform, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetFVF(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	ok(SUCCEEDED(hr), "Failed to set fvf, hr %#x.\n", hr);

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

	
	//for (i = 0; i < 1; ++i)
	for (i = 0; i < ARRAY_SIZE(tests); ++i)
	{
		hr = IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x000000ff, 1.0f, 0);
		ok(SUCCEEDED(hr), "Failed to clear, hr %#x.\n", hr);

		quad[0].position.z = tests[i].z;
		quad[1].position.z = tests[i].z;
		quad[2].position.z = tests[i].z;
		quad[3].position.z = tests[i].z;
		quad[0].position.w = tests[i].w;
		quad[1].position.w = tests[i].w;
		quad[2].position.w = tests[i].w;
		quad[3].position.w = tests[i].w;
		hr = IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, tests[i].z_test);
		ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);

		hr = IDirect3DDevice9_BeginScene(device);
		ok(SUCCEEDED(hr), "Failed to begin scene, hr %#x.\n", hr);
		hr = IDirect3DDevice9_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
		ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);
		hr = IDirect3DDevice9_EndScene(device);
		ok(SUCCEEDED(hr), "Failed to end scene, hr %#x.\n", hr);

		color = getPixelColor(device, 320, 240);
		ok(color_match(color, tests[i].color, 2),
			"Got unexpected color 0x%08x, expected 0x%08x, case %u.\n", color, tests[i].color, i);
		hr = IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL);
		ok(SUCCEEDED(hr), "Failed to present, hr %#x.\n", hr);
	}

			lastTime = currTime;
		}
	}


done:
	refcount = IDirect3DDevice9_Release(device);
	ok(!refcount, "Device has %u references left.\n", refcount);
	IDirect3D9_Release(d3d);
	DestroyWindow(window);
	return 0;
}
