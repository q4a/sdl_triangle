
#include "d3d_utility.h"

#include <cstring>
#include <filesystem>
#include <fstream>

#include <SDL2/SDL.h>
#include <d3dcompiler.h>

// Globals

IDirect3DDevice9* Device = 0;
const int Width = 640;
const int Height = 480;
const int SECOND = 1000;

IDirect3DVertexBuffer9* Triangle = 0;
IDirect3DVertexShader9* ShaderVS = 0;
IDirect3DPixelShader9* ShaderPS = 0;

// Classes and Structures

struct Vertex
{
	Vertex() {}

	Vertex(float x, float y, float z, uint32_t color)
	{
		_x = x;	 _y = y;  _z = z; _color = color;
	}

	float _x, _y, _z;
	uint32_t _color;

	static const uint32_t FVF;
};
const uint32_t Vertex::FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE;

// Additional functions

bool LoadFile(const char* file_name, char** ppBuffer, uint32_t* dwSize)
{
	if (ppBuffer == nullptr)
		return false;

	std::ifstream fileS(file_name, std::ios::binary);
	if (!fileS.is_open())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Can't open file", nullptr);
		return false;
	}
	const auto dwLowSize = std::filesystem::file_size(file_name);
	if (dwSize)
	{
		*dwSize = dwLowSize;
	}
	if (dwLowSize == 0)
	{
		*ppBuffer = nullptr;
		return false;
	}

	*ppBuffer = new char[dwLowSize];
	fileS.exceptions(std::fstream::failbit | std::fstream::badbit);
	fileS.read(*ppBuffer, dwLowSize);
	fileS.close();
	return true;
}

// Framework Functions

bool Setup()
{
	// Create the vertex buffer.

	Device->CreateVertexBuffer(
		3 * sizeof(Vertex), // size in bytes
		D3DUSAGE_WRITEONLY, // flags
		Vertex::FVF,        // vertex format
		D3DPOOL_MANAGED,    // managed memory pool
		&Triangle,          // return create vertex buffer
		0);                 // not used - set to 0

	// Fill the buffers with the triangle data.

	Vertex* vertices;
	Triangle->Lock(0, 0, (void**)&vertices, 0);

	vertices[0] = Vertex(-1.0f, -1.0f, 0.0f, 0xff0000ff);
	vertices[1] = Vertex( 0.0f,  1.0f, 0.0f, 0xff00ff00);
	vertices[2] = Vertex( 1.0f, -1.0f, 0.0f, 0xffff0000);

	Triangle->Unlock();

	// Vertex shader

	HRESULT hr = 0;
	ID3DBlob* shader;
	ID3DBlob* errorMsg;

	char* data = nullptr;
	uint32_t size = 0;
	if (LoadFile("../../../src/min.vs", &data, &size) == false || !data)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Can't load VS file", nullptr);
		return false;
	}

	hr = D3DCompile(data, size, nullptr, nullptr, nullptr, "Main", "vs_1_1", 0, 0, &shader, &errorMsg);
	if (errorMsg)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", (char*)errorMsg->GetBufferPointer(), nullptr);
		d3d::Release<ID3DBlob*>(errorMsg);
	}

	if (FAILED(hr))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "D3DCompile() - FAILED for VS", nullptr);
		return false;
	}

	hr = Device->CreateVertexShader(
		(DWORD*)shader->GetBufferPointer(),
		&ShaderVS);

	if (FAILED(hr))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "CreateVertexShader - FAILED", nullptr);
		return false;
	}

	// Pixel shader

	delete[] data;
	data = nullptr;
	size = 0;
	if (LoadFile("../../../src/min.ps", &data, &size) == false || !data)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Can't load PS file", nullptr);
		return false;
	}

	hr = D3DCompile(data, size, nullptr, nullptr, nullptr, "Main", "ps_2_0", 0, 0, &shader, &errorMsg);
	if (errorMsg)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", (char*)errorMsg->GetBufferPointer(), nullptr);
		d3d::Release<ID3DBlob*>(errorMsg);
	}

	if (FAILED(hr))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "D3DCompile() - FAILED for PS", nullptr);
		return false;
	}

	hr = Device->CreatePixelShader(
		(DWORD*)shader->GetBufferPointer(),
		&ShaderPS);

	if (FAILED(hr))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "CreatePixelShader - FAILED", nullptr);
		return false;
	}

	delete[] data;
	d3d::Release<ID3DBlob*>(shader);

	return true;
}

void Cleanup()
{
	d3d::Release<IDirect3DVertexBuffer9*>(Triangle);
	d3d::Release<IDirect3DVertexShader9*>(ShaderVS);
	d3d::Release<IDirect3DPixelShader9*>(ShaderPS);
}

void ShowPrimitive()
{
	if (Device)
	{
		Device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffffffff, 1.0f, 0);
		Device->BeginScene();

		Device->SetStreamSource(0, Triangle, 0, sizeof(Vertex));
		Device->SetFVF(Vertex::FVF);

		Device->SetVertexShader(ShaderVS);
		Device->SetPixelShader(ShaderPS);
		Device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

		Device->EndScene();
		Device->Present(0, 0, 0, 0);
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

// main ... The main function, right now it just calls the initialization of SDL.
int main(int argc, char* argv[]) {
	//Calling the SDL init stuff.
	initSDL();

	//Creating the context for SDL2.
	SDL_Window* Window = createWindowContext("Hello World!");

	if (!d3d::InitD3D(Window,
		Width, Height, true, D3DDEVTYPE_HAL, &Device))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "InitD3D() - FAILED", nullptr);
		return 0;
	}

	if (!Setup())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Setup() - FAILED", nullptr);
		return 0;
	}

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
	Device->Release();
	SDL_Quit();

	return 0;
}
