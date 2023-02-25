
#include "d3dUtility.h"

#include <fstream>
#include <filesystem>

#include <d3dcompiler.h>

//
// Globals
//

IDirect3DDevice9* Device = 0;

const int Width  = 640;
const int Height = 480;

IDirect3DVertexBuffer9* Triangle = 0;
IDirect3DVertexShader9* ShaderVS = 0;
IDirect3DPixelShader9* ShaderPS = 0;

//
// Classes and Structures
//

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

bool LoadFile(const char* file_name, char** ppBuffer, uint32_t* dwSize)
{
    if (ppBuffer == nullptr)
        return false;

    std::ifstream fileS(file_name, std::ios::binary);
    if (!fileS.is_open())
    {
        ::MessageBox(0, "Can't open file", 0, 0);
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

//
// Framework Functions
//
bool Setup()
{
    //
    // Create the vertex buffer.
    //

    if (FAILED(Device->CreateVertexBuffer(
        3 * sizeof(Vertex), // size in bytes
        D3DUSAGE_WRITEONLY, // flags
        Vertex::FVF,        // vertex format
        D3DPOOL_MANAGED,    // managed memory pool
        &Triangle,          // return create vertex buffer
        0)                  // not used - set to 0
    ))
        return false;

    //
    // Fill the buffers with the triangle data.
    //

    Vertex* pVertices;
    if (FAILED(Triangle->Lock(0, 0, (void**)&pVertices, 0)))
        return false;

    pVertices[0] = Vertex(-1.0f, -1.0f, 0.0f, 0xff0000ff);
    pVertices[1] = Vertex( 0.0f,  1.0f, 0.0f, 0xff00ff00);
    pVertices[2] = Vertex( 1.0f, -1.0f, 0.0f, 0xffff0000);

    Triangle->Unlock();

    //
    // Vertex shader
    //

    HRESULT hr = 0;
    ID3DBlob* shader;
    ID3DBlob* errorMsg;

    char* data = nullptr;
    uint32_t size = 0;
    if (LoadFile("../../../src/min.vs", &data, &size) == false || !data)
    {
        ::MessageBox(0, "Can't load VS file", 0, 0);
        return false;
    }

    hr = D3DCompile(data, size, nullptr, nullptr, nullptr, "Main", "vs_1_1", 0, 0, &shader, &errorMsg);
    if (errorMsg)
    {
        ::MessageBox(0, (char*)errorMsg->GetBufferPointer(), 0, 0);
        d3d::Release<ID3DBlob*>(errorMsg);
    }

    if (FAILED(hr))
    {
        ::MessageBox(0, "D3DCompile() - FAILED for VS", 0, 0);
        return false;
    }

    hr = Device->CreateVertexShader(
        (DWORD*)shader->GetBufferPointer(),
        &ShaderVS);

    if (FAILED(hr))
    {
        ::MessageBox(0, "CreateVertexShader - FAILED", 0, 0);
        return false;
    }

    //
    // Pixel shader
    //

    delete[] data;
    data = nullptr;
    size = 0;
    if (LoadFile("../../../src/min.ps", &data, &size) == false || !data)
    {
        ::MessageBox(0, "Can't load PS file", 0, 0);
        return false;
    }

    hr = D3DCompile(data, size, nullptr, nullptr, nullptr, "Main", "ps_2_0", 0, 0, &shader, &errorMsg);
    if (errorMsg)
    {
        ::MessageBox(0, (char*)errorMsg->GetBufferPointer(), 0, 0);
        d3d::Release<ID3DBlob*>(errorMsg);
    }

    if (FAILED(hr))
    {
        ::MessageBox(0, "D3DCompile() - FAILED for PS", 0, 0);
        return false;
    }

    hr = Device->CreatePixelShader(
        (DWORD*)shader->GetBufferPointer(),
        &ShaderPS);

    if (FAILED(hr))
    {
        ::MessageBox(0, "CreatePixelShader - FAILED", 0, 0);
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

bool Display(float timeDelta)
{
    if (Device)
    {
        //
        // Render
        //

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
    return true;
}

//
// WndProc
//
LRESULT CALLBACK d3d::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch( msg )
	{
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
		
	case WM_KEYDOWN:
		if( wParam == VK_ESCAPE )
			::DestroyWindow(hwnd);
		break;
	}
	return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

//
// WinMain
//
int WINAPI WinMain(HINSTANCE hinstance,
				   HINSTANCE prevInstance, 
				   PSTR cmdLine,
				   int showCmd)
{
	if(!d3d::InitD3D(hinstance,
		Width, Height, true, D3DDEVTYPE_HAL, &Device))
	{
		::MessageBox(0, "InitD3D() - FAILED", 0, 0);
		return 0;
	}
		
	if(!Setup())
	{
		::MessageBox(0, "Setup() - FAILED", 0, 0);
		return 0;
	}

	d3d::EnterMsgLoop( Display );

	Cleanup();

	Device->Release();

	return 0;
}
