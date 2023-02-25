
#include "d3dUtility.h"

//
// Globals
//

IDirect3DDevice9* Device = 0;

const int Width  = 640;
const int Height = 480;

IDirect3DVertexBuffer9* g_lpVertexBuffer = 0;
ID3DXEffect* g_lpEffect;

//
// Classes and Structures
//

//Definition of the Vertex Format including position and diffuse color
#define D3DFVF_COLOREDVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE)

struct COLORED_VERTEX
{
    float x, y, z;  //Position
    uint32_t color; //Color
};

//
// Framework Functions
//
bool Setup()
{
    //set the vertex buffer size 4 vertices * vertex structure size
    UINT uiBufferSize = 4 * sizeof(COLORED_VERTEX);

    //create the buffer
    if (FAILED(Device->CreateVertexBuffer(uiBufferSize,
        D3DUSAGE_WRITEONLY, D3DFVF_COLOREDVERTEX, D3DPOOL_DEFAULT, &g_lpVertexBuffer, NULL)))
        return E_FAIL;

    COLORED_VERTEX* pVertices;
    //lock the buffer for writing
    if (FAILED(g_lpVertexBuffer->Lock(0, uiBufferSize, (void**)&pVertices, 0)))
        return E_FAIL;

    //write the vertices. Here a simple rectangle
    pVertices[0].x = -1.0f; //left
    pVertices[0].y = -1.0f; //bottom
    pVertices[0].z = 0.0f;
    pVertices[0].color = 0xffff0000; //red

    pVertices[1].x = -1.0f; //left
    pVertices[1].y = 1.0f; //top
    pVertices[1].z = 0.0f;
    pVertices[1].color = 0xff0000ff; //blue

    pVertices[2].x = 1.0f; //right
    pVertices[2].y = -1.0f; //bottom
    pVertices[2].z = 0.0f;
    pVertices[2].color = 0xff00ff00; //green

    pVertices[3].x = 1.0f; //right
    pVertices[3].y = 1.0f; //top 
    pVertices[3].z = 0.0f;
    pVertices[3].color = 0xffffffff; //white

    //unlock the buffer
    g_lpVertexBuffer->Unlock();

    //set the Vertex Format
    Device->SetFVF(D3DFVF_COLOREDVERTEX);

    //transfer the buffer to the gpu
    Device->SetStreamSource(0, g_lpVertexBuffer, 0, sizeof(COLORED_VERTEX));

    //create an effect
    ID3DXBuffer* errorBuffer = 0;

    HRESULT hr = D3DXCreateEffectFromFile(
            Device,
            // path from build/msvc-*/bin
            "../../../src/MinimalEffect.fx",
            NULL,
            NULL,
            D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY,
            NULL,
            &g_lpEffect,
            &errorBuffer
    );

    if (errorBuffer)
    {
        ::MessageBox(0, (char*)errorBuffer->GetBufferPointer(), 0, 0);
        d3d::Release<ID3DXBuffer*>(errorBuffer);
    }

    if (FAILED(hr))
    {
        ::MessageBox(0, "D3DXCreateEffectFromFile() - FAILED", 0, 0);
        return false;
    }

    // Choice the tehnique of shader
    D3DXHANDLE hTechnick;
    hr = g_lpEffect->FindNextValidTechnique(NULL, &hTechnick);
    if (FAILED(hr))
    {
        ::MessageBox(0, "FindNextValidTechnique] No finded any valid tehnique in shader effect.", 0, 0);
        return false;
    }
    g_lpEffect->SetTechnique(hTechnick);

    return true;
}
void Cleanup()
{
	d3d::Release<IDirect3DVertexBuffer9*>(g_lpVertexBuffer);
    d3d::Release<ID3DXEffect*>(g_lpEffect);
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

        //begin the effect
        UINT uiPasses = 0;
        g_lpEffect->Begin(&uiPasses, 0);
        for (UINT uiPass = 0; uiPass < uiPasses; uiPass++)
        {
            //render an effect pass
            g_lpEffect->BeginPass(uiPass);

            //render the rectangle
            Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

            g_lpEffect->EndPass();
        }
        g_lpEffect->End();

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
