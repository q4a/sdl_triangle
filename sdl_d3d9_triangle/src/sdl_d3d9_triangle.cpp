
#include "d3d_utility.h"

#include <string.h>
#include <SDL2/SDL.h>

// Globals

IDirect3DDevice9* device = 0;
const int Width = 640;
const int Height = 480;
const int SECOND = 1000;

IDirect3DVertexBuffer9* Triangle = 0; // vertex buffer to store
									  // our triangle data.

// Classes and Structures

struct Vertex
{
	Vertex() {}

	Vertex(float x, float y, float z)
	{
		_x = x;	 _y = y;  _z = z;
	}

	float _x, _y, _z;

	static const DWORD FVF;
};
const DWORD Vertex::FVF = D3DFVF_XYZ;

// Additional math functions

static inline D3DMATRIX* MatrixIdentity(D3DMATRIX* pout)
{
	if (!pout) return nullptr;
	pout->m[0][1] = 0.0f;
	pout->m[0][2] = 0.0f;
	pout->m[0][3] = 0.0f;
	pout->m[1][0] = 0.0f;
	pout->m[1][2] = 0.0f;
	pout->m[1][3] = 0.0f;
	pout->m[2][0] = 0.0f;
	pout->m[2][1] = 0.0f;
	pout->m[2][3] = 0.0f;
	pout->m[3][0] = 0.0f;
	pout->m[3][1] = 0.0f;
	pout->m[3][2] = 0.0f;
	pout->m[0][0] = 1.0f;
	pout->m[1][1] = 1.0f;
	pout->m[2][2] = 1.0f;
	pout->m[3][3] = 1.0f;
	return pout;
}

D3DMATRIX* MatrixPerspectiveFovLH(D3DMATRIX* pout, float fovy, float aspect, float zn, float zf)
{
	MatrixIdentity(pout);
	pout->m[0][0] = 1.0f / (aspect * tanf(fovy / 2.0f));
	pout->m[1][1] = 1.0f / tanf(fovy / 2.0f);
	pout->m[2][2] = zf / (zf - zn);
	pout->m[2][3] = 1.0f;
	pout->m[3][2] = (zf * zn) / (zn - zf);
	pout->m[3][3] = 0.0f;
	return pout;
}

// Framework Functions

bool Setup()
{
	// Create the vertex buffer.

	device->CreateVertexBuffer(
		3 * sizeof(Vertex), // size in bytes
		D3DUSAGE_WRITEONLY, // flags
		Vertex::FVF,        // vertex format
		D3DPOOL_MANAGED,    // managed memory pool
		&Triangle,          // return create vertex buffer
		0);                 // not used - set to 0

	// Fill the buffers with the triangle data.

	Vertex* vertices;
	Triangle->Lock(0, 0, (void**)&vertices, 0);

	vertices[0] = Vertex(-1.0f, 0.5f, 2.0f);
	vertices[1] = Vertex(0.0f, 1.0f, 2.0f);
	vertices[2] = Vertex(1.0f, 0.0f, 2.0f);

	Triangle->Unlock();

	// Set the projection matrix.

	D3DMATRIX proj;
	MatrixPerspectiveFovLH(
		&proj,                        // result
		M_PI * 0.5f,                  // 90 - degrees
		(float)Width / (float)Height, // aspect ratio
		1.0f,                         // near plane
		1000.0f);                     // far plane
	device->SetTransform(D3DTS_PROJECTION, &proj);

	// Set wireframe mode render state.

	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

	return true;
}

void Cleanup()
{
	d3d::Release<IDirect3DVertexBuffer9*>(Triangle);
}

void ShowPrimitive()
{
	if (device)
	{
		device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffffffff, 1.0f, 0);
		device->BeginScene();

		device->SetStreamSource(0, Triangle, 0, sizeof(Vertex));
		device->SetFVF(Vertex::FVF);

		// Draw one triangle.
		device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

		device->EndScene();
		device->Present(0, 0, 0, 0);
	}
}

// init ... The init function, it calls the SDL init function.
int initSDL() {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		return -1;
	}

	return 0;
}

// createWindowContext ... Creating the window for later use in rendering and stuff.
SDL_Window* createWindowContext(std::string title) {
	//Declaring the variable the return later.
	SDL_Window* Window = NULL;

	uint32_t flags;
#if defined(_WIN32) || defined(USE_NINE)
	flags = SDL_WINDOW_OPENGL;
#else // for DXVK Native
	flags = SDL_WINDOW_VULKAN;
#endif

	//Creating the window and passing that reference to the previously declared variable.
	Window = SDL_CreateWindow("Hello World!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Width, Height, flags);

	//Returning the newly creted Window context.
	return Window;
}


int __cdecl fprintf2(FILE* stream, const char* format, ...)
{
	char str[1024];

	va_list argptr;
	va_start(argptr, format);
	int ret = vsnprintf(str, sizeof(str), format, argptr);
	va_end(argptr);

	OutputDebugStringA(str);

	return ret;
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define ok(c, ...) {{if (!(c)) fprintf2(stdout, "fail %s ", __func__); else fprintf2(stdout, "succ %s ", __func__); fprintf2(stdout, __VA_ARGS__);}}
#define skip(...) {fprintf2(stdout, "skip "); fprintf2(stdout, __VA_ARGS__);}
#define trace(...) {fprintf2(stdout, "trace ");fprintf2(stdout, __VA_ARGS__);}

struct vec4
{
	float x, y, z, w;
};

struct surface_readback
{
	IDirect3DSurface9* surface;
	D3DLOCKED_RECT locked_rect;
};

static void get_rt_readback(IDirect3DSurface9* surface, struct surface_readback* rb)
{
	//IDirect3DDevice9* device;
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

static void test_table_fog_zw(void)
{
	HRESULT hr;
	//IDirect3DDevice9* device;
	//IDirect3D9* d3d;
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
		{{  0.0f,   0.0f, 0.0f, 0.0f}, 0xffff0000},
		{{640.0f,   0.0f, 0.0f, 0.0f}, 0xffff0000},
		{{  0.0f, 480.0f, 0.0f, 0.0f}, 0xffff0000},
		{{640.0f, 480.0f, 0.0f, 0.0f}, 0xffff0000},
	};
	static const D3DMATRIX identity =
	{ {{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}} };
	static const struct
	{
		float z, w;
		D3DZBUFFERTYPE z_test;
		D3DCOLOR color;
	}
	tests[] =
	{
		{0.7f,  0.0f, D3DZB_TRUE,  0x0000b200},
		{0.7f,  0.0f, D3DZB_FALSE, 0x0000b200},
		{0.7f,  0.3f, D3DZB_TRUE,  0x004cb200},
		{0.7f,  0.3f, D3DZB_FALSE, 0x004cb200},
		{0.7f,  3.0f, D3DZB_TRUE,  0x004cb200},
		{0.7f,  3.0f, D3DZB_FALSE, 0x004cb200},
		{0.3f,  0.0f, D3DZB_TRUE,  0x00004c00},
		{0.3f,  0.0f, D3DZB_FALSE, 0x00004c00},
	};
	unsigned int i;

	/*
	window = create_window();
	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	ok(!!d3d, "Failed to create a D3D object.\n");

	if (!(device = create_device(d3d, window, window, TRUE)))
	{
		skip("Failed to create a D3D device, skipping tests.\n");
		IDirect3D9_Release(d3d);
		DestroyWindow(window);
		return;
	}
	*/

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
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_CLIPPING, FALSE);
	ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
	/* Work around an AMD Windows driver bug. Needs a proj matrix applied redundantly. */
	hr = IDirect3DDevice9_SetTransform(device, D3DTS_PROJECTION, &identity);
	ok(SUCCEEDED(hr), "Failed to set projection transform, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	ok(SUCCEEDED(hr), "Failed to set render state, hr %#x.\n", hr);
	hr = IDirect3DDevice9_SetFVF(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	ok(SUCCEEDED(hr), "Failed to set fvf, hr %#x.\n", hr);

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

done:
	hr = 0;
	//refcount = IDirect3DDevice9_Release(device);
	//ok(!refcount, "Device has %u references left.\n", refcount);
	//IDirect3D9_Release(d3d);
	//DestroyWindow(window);
}

// main ... The main function, right now it just calls the initialization of SDL.
int main(int argc, char* argv[]) {
	//Calling the SDL init stuff.
	initSDL();

	//Creating the context for SDL2.
	SDL_Window* Window = createWindowContext("Hello World!");

	if (!d3d::InitD3D(Window,
		Width, Height, true, D3DDEVTYPE_HAL, &device))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "InitD3D() - FAILED", nullptr);
		return 0;
	}

	if (!Setup())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Setup() - FAILED", nullptr);
		return 0;
	}
	test_table_fog_zw();

	bool running = true;
	while (running)
	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev))
		{
			if ((SDL_QUIT == ev.type) ||
				(SDL_KEYDOWN == ev.type && SDL_SCANCODE_ESCAPE == ev.key.keysym.scancode))
			{
				running = false;
				break;
			}
		}
		ShowPrimitive();
	}

	//Cleaning up everything.
	Cleanup();
	device->Release();
	SDL_Quit();

	return 0;
}
