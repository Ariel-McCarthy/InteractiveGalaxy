//	-------------------------------------	organization	------------------------------------------
//		includes
//		global var
///			//program entry
//		DirectX global var						
//
//		winmain()
//			{
//			register
//			create win
//			InitDevice()
//				init DirectX (all global obj)	
//			message loop -> winproc
//				render()						
//			}
///			//message functions
//		OnLBD(..)
//			{
//			stuff
//			}
//		OnMMN(..)
///			//message loop function
//		winproc()
//			{
//			switch-case
//				(OnCreate,OnTimer,OnPaint, OnMM,OnLBD, OnLBU,..)
//			}
//  --------------------------------------------------------------------------------------------------



//1. make a net(mesh) of rectangles (=2 triangles)
//	-> we map the texture coordinates to the whole net object
//
//2. map height to every vertex from a texture (height map)
//	-> have a texture in the vertexshader
//	-> color determines the height of the vertex (pos.y)


#include "ground.h"
//	defines
#define MAX_LOADSTRING 1000
#define TIMER1 111
//	global variables
HINSTANCE hInst;											//	program number = instance
TCHAR szTitle[MAX_LOADSTRING];								//	name in window title
TCHAR szWindowClass[MAX_LOADSTRING];						//	class name of window
HWND hMain = NULL;											//	number of windows = handle window = hwnd
static char MainWin[] = "MainWin";							//	class name
HBRUSH  hWinCol = CreateSolidBrush(RGB(180, 180, 180));		//	a color
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

//	+++++++++++++++++++++++++++++++++++++++++++++++++
//	DIRECTX stuff follows here
//	first the global variables (DirectX Objects)
//	second the initdevice(..) function where we load all DX stuff
//	third the Render() function, the equivalent to the OnPaint(), but not from Windows, from DirectX

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------

// key input
int a_key = 0,
	d_key = 0,
	lastMousePos = 0;
double toScale = 1,
	toTranslateY = 1,
	toTranslateZ = 1;
bool change = true;
camera cam;
XMMATRIX translate = XMMatrixIdentity();
XMMATRIX cameraDelta = XMMatrixIdentity();
XMMATRIX cameraRotate = XMMatrixIdentity();


D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;

// device context
ID3D11Device*           g_pd3dDevice = NULL;			// is for initialization and loading things (pictures, models, ...) <- InitDevice()
ID3D11DeviceContext*    g_pImmediateContext = NULL;		// is for render your models w/ pics on the screen					<- Render()
														// page flipping:
IDXGISwapChain*         g_pSwapChain = NULL;

// screen <- thats our render target
ID3D11RenderTargetView* g_pRenderTargetView = NULL;

// how a vertex looks like
ID3D11InputLayout*      g_pVertexLayout = NULL;
ID3D11InputLayout*		g_pVertexLayoutRGB = NULL;

// our model (array of vertices on the GPU MEM)
ID3D11Buffer*           g_pVertexBuffer = NULL; //our rectangle
ID3D11Buffer*           g_pVertexBufferPlane = NULL; //our plane
ID3D11Buffer*           g_pVertexBufferSky = NULL; //our plane
ID3D11Buffer*			g_pVertexBufferBoxTextured = NULL; // cube
ID3D11Buffer*			g_pVertexBufferStarRGB = NULL; // star
int						g_vertices_star_rgb; // vertex count for star
int						g_vertices_box_textured; // vertex count for cube
int						g_vertices_plane;
int						g_vertices_sky;

// exchange of data, e.g. sending mouse coordinates to the GPU
ID3D11Buffer*			g_pConstantBuffer11 = NULL;

// function on the GPU what to do with the model exactly
ID3D11VertexShader*     g_pVertexShader = NULL;
ID3D11VertexShader*     g_pVertexShaderRGB = NULL;
ID3D11PixelShader*		g_pPixelShaderRGB = NULL;
ID3D11PixelShader*      g_pPixelShader = NULL;
ID3D11PixelShader*      g_pPixelShaderSky = NULL;

// depth stencli buffer
ID3D11Texture2D*                    g_pDepthStencil = NULL;
ID3D11DepthStencilView*             g_pDepthStencilView = NULL;

// for transparency:
ID3D11BlendState*					g_BlendState;

// Texture
ID3D11ShaderResourceView*           g_texBackground = NULL;
ID3D11ShaderResourceView*           g_TexPlane = NULL;
ID3D11ShaderResourceView*           g_HeightMap = NULL;
ID3D11ShaderResourceView*           g_HeightMap2 = NULL;
ID3D11ShaderResourceView*           g_earth = NULL;
ID3D11ShaderResourceView*           g_texBackground2 = NULL;
ID3D11ShaderResourceView*           g_night = NULL;
ID3D11ShaderResourceView*           g_moon = NULL;
ID3D11ShaderResourceView*           g_texMoon = NULL;
ID3D11ShaderResourceView*           g_texCube = NULL;
ID3D11ShaderResourceView*			g_texJupiter = NULL;

// Texture Sampler
ID3D11SamplerState*                 g_Sampler = NULL;

// rasterizer states
ID3D11RasterizerState				*rs_CW, *rs_CCW, *rs_NO, *rs_Wire;

// depth state
ID3D11DepthStencilState				*ds_on, *ds_off;

//structs we need later
XMMATRIX g_world;//model: per object position and rotation and scaling of the object
XMMATRIX g_view;//camera: position and rotation of the camera
XMMATRIX g_projection;//perspective: angle of view, near plane / far plane

struct VS_CONSTANT_BUFFER
	{
	float some_variable_a;
	float some_variable_b;
	float some_variable_c;
	float some_variable_d;
	float div_tex_x;	//dividing of the texture coordinates in x
	float div_tex_y;	//dividing of the texture coordinates in x
	float slice_x;		
	float slice_y;		
	XMMATRIX world; //model: per object position and rotation and scaling of the object
	XMMATRIX view; //camera: position and rotation of the camera
	XMMATRIX projection; //perspective: angle of view, near plane / far plane
	XMFLOAT4 lightposition; // light position
	XMFLOAT4 camposition; // camera position



	};	//we will copy that periodically to its twin on the GPU, with some_variable_a/b for the mouse coordinates
		//note: we can only copy chunks of 16 byte to the GPU
VS_CONSTANT_BUFFER VsConstData;		//global object of this structure


					//--------------------------------------------------------------------------------------
					// Create Direct3D device and swap chain
					//--------------------------------------------------------------------------------------
HRESULT InitDevice()
	{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(hMain, &rc);	
	
	//getting the windows size into a RECT structure
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
		{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
		};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
		{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hMain;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
		{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		if (SUCCEEDED(hr))
			break;
		}
	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	// ************ textures *************

	// Compile the vertex shader
	ID3DBlob* pVSBlob = NULL;
	hr = CompileShaderFromFile(L"shader.fx", "VShader", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
		{
		MessageBox(NULL,
			L"The FX file cannot be compiled. Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
		}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr))
		{
		pVSBlob->Release();
		return hr;
		}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
		{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
	UINT numElements = ARRAYSIZE(layout);


	// Create the input layout 
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;



	// ******** RGB ***********

	// Compile the vertex shader RGB
	pVSBlob = NULL;
	hr = CompileShaderFromFile(L"shader.fx", "VShaderRGB", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The FX file cannot be compiled. Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader RGB
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShaderRGB);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Define the input layout RGB
	D3D11_INPUT_ELEMENT_DESC layout2[] =
	{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	numElements = ARRAYSIZE(layout2);

	// Create the input layout RGB
	hr = g_pd3dDevice->CreateInputLayout(layout2, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayoutRGB);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the pixel shader 
	ID3DBlob* pPSBlob = NULL;
	hr = CompileShaderFromFile(L"shader.fx", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
		{
		MessageBox(NULL,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
		}

	// Create the pixel shader 
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the pixel shader RGB
	pPSBlob = NULL;
	hr = CompileShaderFromFile(L"shader.fx", "PShaderRGB", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The FX file cannot be compiled. Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader RGB
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShaderRGB);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// sky

	// compile pixel shader
	pPSBlob = NULL;
	hr = CompileShaderFromFile(L"shader.fx", "PSsky", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
		{
		MessageBox(NULL,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
		}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShaderSky);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;


	D3D11_BUFFER_DESC bd;
	D3D11_SUBRESOURCE_DATA InitData;

	
	// vertices for cube
	g_vertices_box_textured = 36;
	SimpleVertex* texturedCube = new SimpleVertex[g_vertices_box_textured];

	// front
	texturedCube[0].Pos = XMFLOAT3(-1.0, 1.0, -1.0);
	texturedCube[0].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[0].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	texturedCube[1].Pos = XMFLOAT3(1.0, 1.0, -1.0);
	texturedCube[1].Tex = XMFLOAT2(1.0, 0.0);
	texturedCube[1].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	texturedCube[2].Pos = XMFLOAT3(1.0, -1.0, -1.0);
	texturedCube[2].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[2].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	texturedCube[3].Pos = XMFLOAT3(-1.0, 1.0, -1.0);
	texturedCube[3].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[3].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	texturedCube[4].Pos = XMFLOAT3(1.0, -1.0, -1.0);
	texturedCube[4].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[4].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	texturedCube[5].Pos = XMFLOAT3(-1.0, -1.0, -1.0);
	texturedCube[5].Tex = XMFLOAT2(0.0, 1.0);
	texturedCube[5].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	// left
	texturedCube[6].Pos = XMFLOAT3(-1.0, -1.0, 1.0);
	texturedCube[6].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[6].Norm = XMFLOAT3(-1.0, 0.0, 0.0);

	texturedCube[7].Pos = XMFLOAT3(-1.0, 1.0, 1.0);
	texturedCube[7].Tex = XMFLOAT2(1.0, 0.0);
	texturedCube[7].Norm = XMFLOAT3(-1.0, 0.0, 0.0);

	texturedCube[8].Pos = XMFLOAT3(-1.0, 1.0, -1.0);
	texturedCube[8].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[8].Norm = XMFLOAT3(-1.0, 0.0, 0.0);

	texturedCube[9].Pos = XMFLOAT3(-1.0, -1.0, 1.0);
	texturedCube[9].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[9].Norm = XMFLOAT3(-1.0, 0.0, 0.0);

	texturedCube[10].Pos = XMFLOAT3(-1.0, 1.0, -1.0);
	texturedCube[10].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[10].Norm = XMFLOAT3(-1.0, 0.0, 0.0);

	texturedCube[11].Pos = XMFLOAT3(-1.0, -1.0, -1.0);
	texturedCube[11].Tex = XMFLOAT2(0.0, 1.0);
	texturedCube[11].Norm = XMFLOAT3(-1.0, 0.0, 0.0);

	// back
	texturedCube[12].Pos = XMFLOAT3(1.0, 1.0, 1.0);
	texturedCube[12].Tex = XMFLOAT2(1.0, 0.0);
	texturedCube[12].Norm = XMFLOAT3(0.0, 0.0, 1.0);

	texturedCube[13].Pos = XMFLOAT3(-1.0, 1.0, 1.0);
	texturedCube[13].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[13].Norm = XMFLOAT3(0.0, 0.0, 1.0);

	texturedCube[14].Pos = XMFLOAT3(1.0, -1.0, 1.0);
	texturedCube[14].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[14].Norm = XMFLOAT3(0.0, 0.0, 1.0);

	texturedCube[15].Pos = XMFLOAT3(1.0, -1.0, 1.0);
	texturedCube[15].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[15].Norm = XMFLOAT3(0.0, 0.0, 1.0);

	texturedCube[16].Pos = XMFLOAT3(-1.0, 1.0, 1.0);
	texturedCube[16].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[16].Norm = XMFLOAT3(0.0, 0.0, 1.0);

	texturedCube[17].Pos = XMFLOAT3(-1.0, -1.0, 1.0);
	texturedCube[17].Tex = XMFLOAT2(0.0, 1.0);
	texturedCube[17].Norm = XMFLOAT3(0.0, 0.0, 1.0);

	// right
	texturedCube[18].Pos = XMFLOAT3(1.0, 1.0, 1.0);
	texturedCube[18].Tex = XMFLOAT2(1.0, 0.0);
	texturedCube[18].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[19].Pos = XMFLOAT3(1.0, -1.0, 1.0);
	texturedCube[19].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[19].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[20].Pos = XMFLOAT3(1.0, 1.0, -1.0);
	texturedCube[20].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[20].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[21].Pos = XMFLOAT3(1.0, 1.0, -1.0);
	texturedCube[21].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[21].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[22].Pos = XMFLOAT3(1.0, -1.0, 1.0);
	texturedCube[22].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[22].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[23].Pos = XMFLOAT3(1.0, -1.0, -1.0);
	texturedCube[23].Tex = XMFLOAT2(0.0, 1.0);
	texturedCube[23].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	// bottom
	texturedCube[24].Pos = XMFLOAT3(1.0, -1.0, -1.0);
	texturedCube[24].Tex = XMFLOAT2(1.0, 0.0);
	texturedCube[24].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[25].Pos = XMFLOAT3(1.0, -1.0, 1.0);
	texturedCube[25].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[25].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[26].Pos = XMFLOAT3(-1.0, -1.0, -1.0);
	texturedCube[26].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[26].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[27].Pos = XMFLOAT3(-1.0, -1.0, -1.0);
	texturedCube[27].Tex = XMFLOAT2(1.0, 1.0);
	texturedCube[27].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[28].Pos = XMFLOAT3(1.0, -1.0, 1.0);
	texturedCube[28].Tex = XMFLOAT2(0.0, 0.0);
	texturedCube[28].Norm = XMFLOAT3(1.0, 0.0, 0.0);

	texturedCube[29].Pos = XMFLOAT3(-1.0, -1.0, 1.0);
	texturedCube[29].Tex = XMFLOAT2(0.0, 1.0);
	texturedCube[29].Norm = XMFLOAT3(1.0, 0.0, 0.0);


	// set aside memory for vertex buffer
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * g_vertices_box_textured;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = texturedCube;

	// create vertex buffer
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBufferBoxTextured);
	if (FAILED(hr))
		return hr;
	

	/*
	// Create the shape
	SimpleVertex vertices[6];
	vertices[0].Pos = XMFLOAT3(-1, 1, 1);	
	vertices[1].Pos = XMFLOAT3(1, -1, 1);	
	vertices[2].Pos = XMFLOAT3(-1, -1, 1); 
	vertices[3].Pos = XMFLOAT3(-1, 1, 1);	
	vertices[4].Pos = XMFLOAT3(1, 1, 1);	
	vertices[5].Pos = XMFLOAT3(1, -1, 1);	

	/* since images are 2D (the object shape simply 
	   makes them look 3D) I only need x, y coords 
	vertices[0].Tex = XMFLOAT2(0.0f, 0.0f);
	vertices[1].Tex = XMFLOAT2(1.0f, 1.0f);
	vertices[2].Tex = XMFLOAT2(0.0f, 1.0f);
	vertices[3].Tex = XMFLOAT2(0.0f, 0.0f);			
	vertices[4].Tex = XMFLOAT2(1.0f, 0.0f);			
	vertices[5].Tex = XMFLOAT2(1.0f, 1.0f);				

	// set aside memory for vertex buffer
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 6;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = vertices;

	// create vertex buffer
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
	if (FAILED(hr))
		return hr;
	*/
	

	g_vertices_star_rgb = 36;
	SimpleVertexRGB* rgbStar = new SimpleVertexRGB[g_vertices_star_rgb];

	// top point
	rgbStar[0].Pos = XMFLOAT3(-0.27, 0.27, -1.0);
	rgbStar[0].Col = XMFLOAT4(1.0, 0.0, 0.0, 1.0);
	rgbStar[0].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[1].Pos = XMFLOAT3(0.0, 1.0, -1.0);
	rgbStar[1].Col = XMFLOAT4(0.0, 1.0, 0.0, 1.0);
	rgbStar[1].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[2].Pos = XMFLOAT3(0.27, 0.27, -1.0);
	rgbStar[2].Col = XMFLOAT4(0.0, 0.0, 1.0, 1.0);
	rgbStar[2].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	// right point
	rgbStar[3].Pos = XMFLOAT3(0.27, 0.27, -1.0);
	rgbStar[3].Col = XMFLOAT4(1.0, 0.0, 0.0, 1.0);
	rgbStar[3].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[4].Pos = XMFLOAT3(1.0, 0.0, -1.0);
	rgbStar[4].Col = XMFLOAT4(0.0, 1.0, 0.0, 1.0);
	rgbStar[4].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[5].Pos = XMFLOAT3(0.27, -0.27, -1.0);
	rgbStar[5].Col = XMFLOAT4(0.0, 0.0, 1.0, 1.0);
	rgbStar[5].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	// bottom right point
	rgbStar[6].Pos = XMFLOAT3(0.27, -0.27, -1.0);
	rgbStar[6].Col = XMFLOAT4(1.0, 0.0, 0.0, 1.0);
	rgbStar[6].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[7].Pos = XMFLOAT3(0.6, -1.0, -1.0);
	rgbStar[7].Col = XMFLOAT4(0.0, 1.0, 0.0, 1.0);
	rgbStar[7].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[8].Pos = XMFLOAT3(0.0, -0.6, -1.0);
	rgbStar[8].Col = XMFLOAT4(0.0, 0.0, 1.0, 1.0);
	rgbStar[8].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	// bottom left point
	rgbStar[9].Pos = XMFLOAT3(0.0, -0.6, -1.0);
	rgbStar[9].Col = XMFLOAT4(1.0, 0.0, 1.0, 1.0);
	rgbStar[9].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[10].Pos = XMFLOAT3(-0.6, -1.0, -1.0);
	rgbStar[10].Col = XMFLOAT4(0.0, 1.0, 0.0, 1.0);
	rgbStar[10].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[11].Pos = XMFLOAT3(-0.27, -0.27, -1.0);
	rgbStar[11].Col = XMFLOAT4(0.0, 0.0, 1.0, 1.0);
	rgbStar[11].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	// left point
	rgbStar[12].Pos = XMFLOAT3(-0.27, -0.27, -1.0);
	rgbStar[12].Col = XMFLOAT4(1.0, 0.0, 0.0, 1.0);
	rgbStar[12].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[13].Pos = XMFLOAT3(-1.0, 0.0, -1.0);
	rgbStar[13].Col = XMFLOAT4(0.0, 1.0, 0.0, 1.0);
	rgbStar[13].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[14].Pos = XMFLOAT3(-0.27, 0.27, -1.0);
	rgbStar[14].Col = XMFLOAT4(0.0, 0.0, 1.0, 1.0);
	rgbStar[14].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	// inner right triangle
	// bottom right point
	rgbStar[15].Pos = XMFLOAT3(-0.27, 0.27, -1.0);
	rgbStar[15].Col = XMFLOAT4(1.0, 0.0, 0.0, 1.0);
	rgbStar[15].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[16].Pos = XMFLOAT3(0.27, 0.27, -1.0);
	rgbStar[16].Col = XMFLOAT4(0.0, 1.0, 0.0, 1.0);
	rgbStar[16].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[17].Pos = XMFLOAT3(0.27, -0.27, -1.0);
	rgbStar[17].Col = XMFLOAT4(0.0, 0.0, 1.0, 1.0);
	rgbStar[17].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	// inner bottom right triangle
	// bottom left point
	rgbStar[18].Pos = XMFLOAT3(0.27, -0.27, -1.0);
	rgbStar[18].Col = XMFLOAT4(1.0, 0.0, 1.0, 1.0);
	rgbStar[18].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[19].Pos = XMFLOAT3(0.0, -0.6, -1.0);
	rgbStar[19].Col = XMFLOAT4(0.0, 1.0, 0.0, 1.0);
	rgbStar[19].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[20].Pos = XMFLOAT3(-0.27, -0.27, -1.0);
	rgbStar[20].Col = XMFLOAT4(0.0, 0.0, 1.0, 1.0);
	rgbStar[20].Norm = XMFLOAT3(0.0, 0.0, -1.0);

	// inner left triangle
	// left point
	rgbStar[21].Pos = XMFLOAT3(-0.27, -0.27, -1.0);
	rgbStar[21].Col = XMFLOAT4(1.0, 0.0, 0.0, 1.0);
	rgbStar[21].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[22].Pos = XMFLOAT3(-0.27, 0.27, -1.0);
	rgbStar[22].Col = XMFLOAT4(0.0, 1.0, 0.0, 1.0);
	rgbStar[22].Norm = XMFLOAT3(0.0, 0.0, -1.0);
	rgbStar[23].Pos = XMFLOAT3(0.27, -0.27, -1.0);
	rgbStar[23].Col = XMFLOAT4(0.0, 0.0, 1.0, 1.0);
	rgbStar[23].Norm = XMFLOAT3(0.0, 0.0, -1.0);


	// set aside memory for vertex buffer
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertexRGB) * g_vertices_star_rgb;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = rgbStar;

	// create vertex buffer
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBufferStarRGB);
	if (FAILED(hr))
		return hr;

	// Supply the vertex shader constant data.
	VsConstData.some_variable_a = 0;
	VsConstData.some_variable_b = 0;
	VsConstData.some_variable_c = 1;
	VsConstData.some_variable_d = 1;
	VsConstData.div_tex_x = 1;
	VsConstData.div_tex_y = 1;
	VsConstData.slice_x = 0;
	VsConstData.slice_y = 0;

	//load the plane into vertex buffer:
	Load3DS("sphere.3ds", g_pd3dDevice, &g_pVertexBufferSky, &g_vertices_sky, FALSE);


	// Fill in a buffer description.
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory(&cbDesc, sizeof(cbDesc));
	cbDesc.ByteWidth = sizeof(VS_CONSTANT_BUFFER);
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = 0;
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = &VsConstData;

	// Create the buffer.
	hr = g_pd3dDevice->CreateBuffer(&cbDesc, &InitData, &g_pConstantBuffer11);
	if (FAILED(hr))
		return hr;

	//Init texture
	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"nightSky.jpeg", NULL, NULL, &g_texBackground, NULL);
	if (FAILED(hr))
		return hr;

	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"earth.jpg", NULL, NULL, &g_earth, NULL);
	if (FAILED(hr))
		return hr;

	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"sky.jpg", NULL, NULL, &g_texBackground2, NULL);
	if (FAILED(hr))
		return hr;

	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"earthnight.jpg", NULL, NULL, &g_night, NULL);
	if (FAILED(hr))
		return hr;

	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"earth.jpg", NULL, NULL, &g_moon, NULL);
	if (FAILED(hr))
		return hr;
	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"moon.jpg", NULL, NULL, &g_texMoon, NULL);
	if (FAILED(hr))
		return hr;

	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"galaxy.jpg", NULL, NULL, &g_texCube, NULL);
	if (FAILED(hr))
		return hr;

	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"jupiter.jpg", NULL, NULL, &g_texJupiter, NULL);
	if (FAILED(hr))
		return hr;

	//Sampler init
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_Sampler);
	if (FAILED(hr))
		return hr;

	//blendstate init
	//blendstate:
	D3D11_BLEND_DESC blendStateDesc;
	ZeroMemory(&blendStateDesc, sizeof(D3D11_BLEND_DESC));
	blendStateDesc.AlphaToCoverageEnable = FALSE;
	blendStateDesc.IndependentBlendEnable = FALSE;
	blendStateDesc.RenderTarget[0].BlendEnable = TRUE;
	blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F;
	g_pd3dDevice->CreateBlendState(&blendStateDesc, &g_BlendState);

	//matrices

	//world
	g_world = XMMatrixIdentity();

	//view:
	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);// camera position
	XMVECTOR At = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);// look at
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);// normal vector on at vector (always up)
	g_view = XMMatrixLookAtLH(Eye, At, Up);

	//perspective:
	// Initialize the projection matrix
	g_projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 10000.0f);

	//depth buffer:

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_R32_TYPELESS;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = DXGI_FORMAT_D32_FLOAT;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	//rasterizer states:
	//setting the rasterizer:
	D3D11_RASTERIZER_DESC			RS_CW, RS_Wire;

	RS_CW.AntialiasedLineEnable = FALSE;
	RS_CW.CullMode = D3D11_CULL_BACK;
	RS_CW.DepthBias = 0;
	RS_CW.DepthBiasClamp = 0.0f;
	RS_CW.DepthClipEnable = true;
	RS_CW.FillMode = D3D11_FILL_SOLID;
	RS_CW.FrontCounterClockwise = false;
	RS_CW.MultisampleEnable = FALSE;
	RS_CW.ScissorEnable = false;
	RS_CW.SlopeScaledDepthBias = 0.0f;

	//rasterizer state clockwise triangles
	g_pd3dDevice->CreateRasterizerState(&RS_CW, &rs_CW);

	//rasterizer state counterclockwise triangles
	RS_CW.CullMode = D3D11_CULL_FRONT;
	g_pd3dDevice->CreateRasterizerState(&RS_CW, &rs_CCW);
	RS_Wire = RS_CW;
	RS_Wire.CullMode = D3D11_CULL_NONE;

	//rasterizer state seeing both sides of the triangle
	g_pd3dDevice->CreateRasterizerState(&RS_Wire, &rs_NO);

	//rasterizer state wirefrime
	RS_Wire.FillMode = D3D11_FILL_WIREFRAME;
	g_pd3dDevice->CreateRasterizerState(&RS_Wire, &rs_Wire);

	//init depth stats:
	//create the depth stencil states for turning the depth buffer on and of:
	D3D11_DEPTH_STENCIL_DESC		DS_ON, DS_OFF;
	DS_ON.DepthEnable = true;
	DS_ON.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DS_ON.DepthFunc = D3D11_COMPARISON_LESS;

	// Stencil test parameters
	DS_ON.StencilEnable = true;
	DS_ON.StencilReadMask = 0xFF;
	DS_ON.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing
	DS_ON.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DS_ON.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	DS_ON.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DS_ON.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing
	DS_ON.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DS_ON.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	DS_ON.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DS_ON.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create depth stencil state
	DS_OFF = DS_ON;
	DS_OFF.DepthEnable = false;
	g_pd3dDevice->CreateDepthStencilState(&DS_ON, &ds_on);
	g_pd3dDevice->CreateDepthStencilState(&DS_OFF, &ds_off);

	return S_OK;
	}

//--------------------------------------------------------------------------------------
// Render function
//--------------------------------------------------------------------------------------

void Render()
{
	//move camera
	//g_view *= cameraDelta;

	//scale object
	g_world *= XMMatrixScaling(1.0, 1.0, 1.0);

	//move object
	//g_world *= translate;

	// Clear the back buffer 
	float ClearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // red,green,blue,alpha
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	float blendFactor[] = { 0, 0, 0, 0 };
	UINT sampleMask = 0xffffffff;
	g_pImmediateContext->OMSetBlendState(g_BlendState, blendFactor, sampleMask);

	// Set primitive topology
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_pImmediateContext->PSSetSamplers(0, 1, &g_Sampler);
	g_pImmediateContext->VSSetSamplers(0, 1, &g_Sampler);

	// textures

	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Render a triangle.. Set vertex and pixel shaders
	g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShaderSky, NULL, 0);

	// Set vertex buffer, setting the model
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
/*
	g_pImmediateContext->RSSetState(rs_CW);

	XMMATRIX Mplane;
*/	
	//change the world matrix
	XMMATRIX V = g_view;
	V = cam.calculate_view(V);
//	VsConstData.projection = g_projection;

	//			************			render the skysphere:				********************
	XMMATRIX Tv = XMMatrixTranslation(cam.pos.x, cam.pos.y, -cam.pos.z);
	XMMATRIX Rx = XMMatrixRotationX(XM_PIDIV2);
	XMMATRIX S = XMMatrixScaling(0.008, 0.008, 0.008);
	XMMATRIX Msky = S*Rx*Tv; //from left to right
	VsConstData.world = Msky;
	VsConstData.view = V;
	VsConstData.projection = g_projection;
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer11, 0, 0, &VsConstData, 0, 0);	//copying it freshly into the GPU buffer from VsConstData
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer11);					//setting it enable for the VertexShader
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer11);					//setting it enable for the PixelShader
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBufferSky, &stride, &offset);
	
	// change background w/ k
	if (change) {
		g_pImmediateContext->PSSetShaderResources(0, 1, &g_texBackground);
	}
	else {
		g_pImmediateContext->PSSetShaderResources(0, 1, &g_texBackground2);
	}
	
	g_pImmediateContext->RSSetState(rs_CCW);//to see it from the inside
	g_pImmediateContext->OMSetDepthStencilState(ds_off, 1);//no depth writing
	g_pImmediateContext->Draw(g_vertices_sky, 0);		//render sky
	g_pImmediateContext->RSSetState(rs_CW);//reset to default
	g_pImmediateContext->OMSetDepthStencilState(ds_on, 1);//reset to default

	
	//		  	************		     	render the planet: Earth				********************

	XMMATRIX T = XMMatrixTranslation(0, 0, 20);
	Rx = XMMatrixRotationX(XM_PIDIV2);
	static float angle = 0;
	angle = angle + 0.0001;

	XMMATRIX Ry = XMMatrixRotationY(angle);
	S = XMMatrixScaling(0.01 * toScale, 0.01 * toScale, 0.01 * toScale);
	Msky = S*Rx*Ry*T; //from left to right

	VsConstData.world = Msky;
	VsConstData.view = V;
	VsConstData.projection = g_projection;
	g_pImmediateContext->PSSetShaderResources(0, 1, &g_earth);
	g_pImmediateContext->PSSetShaderResources(1, 1, &g_earth);
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer11, 0, 0, &VsConstData, 0, 0);	//copying it freshly into the GPU buffer from VsConstData
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer11);					//setting it enable for the VertexShader
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer11);					//setting it enable for the PixelShader
	g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);
	g_pImmediateContext->Draw(g_vertices_sky, 0);		// render Earth

	//                 ****************************                    ***************************

	// render moon

	static float angle2 = 0;
	angle2 = angle2 + 0.00001;
	T = XMMatrixTranslation(0 + 10 * cos(angle), 0, 20 + 10 * sin(angle));
	Rx = XMMatrixRotationX(XM_PIDIV2);

	Ry = XMMatrixRotationY(-2.0f *angle2);
	S = XMMatrixScaling(0.003 * toScale, 0.003 * toScale, 0.003 * toScale);
	Msky = S*Rx*Ry*T;

	VsConstData.world = Msky;
	VsConstData.view = V;
	VsConstData.projection = g_projection;
	g_pImmediateContext->PSSetShaderResources(0, 1, &g_texMoon);
	g_pImmediateContext->PSSetShaderResources(1, 1, &g_texMoon);

	// copying it freshly into the GPU buffer from VsConstData
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer11, 0, 0, &VsConstData, 0, 0);	

	// setting it enable for the VertexShader
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer11);		

	// setting it enable for the PixelShader
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer11);					
	g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

	// render moon
	g_pImmediateContext->Draw(g_vertices_sky, 0);		
													
//              *****************************                           *********************************

	// render Jupiter

	static float angle3 = 0;
	angle3 = angle3 + 0.000001;

	T = XMMatrixTranslation(0 + 27 * cos(angle), 0, 24 + 27 * sin(angle));  
	Rx = XMMatrixRotationX(XM_PIDIV2);

	Ry = XMMatrixRotationY(-2.0f *angle3);
	S = XMMatrixScaling(0.024 * toScale , 0.024 * toScale, 0.024 * toScale);
	Msky = S*Rx*Ry*T;

	VsConstData.world = Msky;
	VsConstData.view = V;
	VsConstData.projection = g_projection;
	g_pImmediateContext->PSSetShaderResources(0, 1, &g_texJupiter);
	g_pImmediateContext->PSSetShaderResources(1, 1, &g_texJupiter);

	// copying it freshly into the GPU buffer from VsConstData
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer11, 0, 0, &VsConstData, 0, 0);

	// setting it enable for the VertexShader
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer11);

	// setting it enable for the PixelShader
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer11);
	g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

	// render Jupiter
	g_pImmediateContext->Draw(g_vertices_sky, 0);

	//              *****************************                           *********************************

	// render cube

	Msky = XMMatrixIdentity();
	T = XMMatrixTranslation(0 + 44 * cos(angle*3), 11 * toTranslateY, 47 + 7 * sin(angle) * toTranslateZ);
	S = XMMatrixScaling(.8 * toScale, .8 * toScale, .8 * toScale);
	Msky = T*S;
	VsConstData.world = Msky;
	VsConstData.view = V;
	VsConstData.projection = g_projection;

	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBufferBoxTextured, &stride, &offset);

	// copying it freshly into the GPU buffer from VsConstData
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer11, 0, 0, &VsConstData, 0, 0);

	// setting it enable for the VertexShader
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer11);

	// setting it enable for the PixelShader
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer11);

	// changing the buffer to my cube
	g_pImmediateContext->PSSetShaderResources(0, 1, &g_texCube);

	// render cube
	g_pImmediateContext->Draw(g_vertices_box_textured, 0);

	//               *************************                   ****************************

	// render RGB star

	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayoutRGB);

	// Render a triangle.. Set vertex and pixel shaders
	g_pImmediateContext->VSSetShader(g_pVertexShaderRGB, NULL, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShaderRGB, NULL, 0);

	// Set vertex buffer, setting the model
	stride = sizeof(SimpleVertexRGB);
	offset = 0;

	// render star
	Msky = XMMatrixIdentity();
	T = XMMatrixTranslation(4, 7 + 4 * tan(angle * 4), 7 + 7 * sin(angle));
	Msky = T;
	VsConstData.world = Msky;
	VsConstData.view = V;
	VsConstData.projection = g_projection;

	// changing the buffer to my star
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBufferStarRGB, &stride, &offset);

	// copying it freshly into the GPU buffer from VsConstData
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer11, 0, 0, &VsConstData, 0, 0);

	// setting it enable for the VertexShader
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer11);

	// setting it enable for the PixelShader
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer11);

	// render star
	g_pImmediateContext->Draw(g_vertices_star_rgb, 0);

	//                ****************************              ******************************
	/*
	// set light and camera position
	VsConstData.lightposition = XMFLOAT4(1000 * sin(angle), 0, 1000 * cos(angle), 0);
	VsConstData.camposition = XMFLOAT4(cam.pos.x, cam.pos.y, cam.pos.z, 0);
*/
	// Present the information rendered to the back buffer to the front buffer (the screen)
	g_pSwapChain->Present(0, 0);
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);	//message loop function (containing all switch-case statements

int WINAPI wWinMain(				//	the main function in a window program. program starts here
	HINSTANCE hInstance,			//	here the program gets its own number
	HINSTANCE hPrevInstance,		//	in case this program is called from within another program
	LPTSTR    lpCmdLine,
	int       nCmdShow)
	{

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	hInst = hInstance;												//	save in global variable for further use
	MSG msg;

	// Globale Zeichenfolgen initialisieren
	LoadString(hInstance, 103, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, 104, szWindowClass, MAX_LOADSTRING);
	//register Window													<<<<<<<<<<			STEP ONE: REGISTER WINDOW						!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	WNDCLASSEX wcex;												//						=> Filling out struct WNDCLASSEX
	BOOL Result = TRUE;
	wcex.cbSize = sizeof(WNDCLASSEX);								//						size of this struct (don't know why
	wcex.style = CS_HREDRAW | CS_VREDRAW;							//						?
	wcex.lpfnWndProc = (WNDPROC)WndProc;							//						The corresponding Proc File -> Message loop switch-case file
	wcex.cbClsExtra = 0;											//
	wcex.cbWndExtra = 0;											//
	wcex.hInstance = hInstance;										//						The number of the program
	wcex.hIcon = LoadIcon(hInstance, NULL);							//
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);						//
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);				//						Background color
	wcex.lpszMenuName = NULL;										//
	wcex.lpszClassName = L"ArielsWindowClass";									//						Name of the window (must the the same as later when opening the window)
	wcex.hIconSm = LoadIcon(wcex.hInstance, NULL);					//
	Result = (RegisterClassEx(&wcex) != 0);							//						Register this struct in the OS

																	//													STEP TWO: OPENING THE WINDOW with x,y position and xlen, ylen !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	RECT rc = { 0, 0, 1920, 1080 };//640,480 ... 1280,720
	hMain = CreateWindow(L"ArielsWindowClass", L"Final Project - Sovreign-Ariel S. McCarthy",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);
	if (hMain == 0)	return 0;

	ShowWindow(hMain, nCmdShow);
	UpdateWindow(hMain);


	if (FAILED(InitDevice()))
		{
		return 0;
		}

	//	STEP THREE: Going into the infinity message loop		

	// Main message loop
	msg = { 0 };
	while (WM_QUIT != msg.message)
		{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			}
		else
			{
			Render();
			}
		}

	return (int)msg.wParam;
	}
///////////////////////////////////////////////////
void redr_win_full(HWND hwnd, bool erase)
	{
	RECT rt;
	GetClientRect(hwnd, &rt);
	InvalidateRect(hwnd, &rt, erase);
	}

///////////////////////////////////
//		This Function is called every time the Left Mouse Button is down
///////////////////////////////////
void OnLBD(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
	{

	}
///////////////////////////////////
//		This Function is called every time the Right Mouse Button is down
///////////////////////////////////
void OnRBD(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
	{

	}
///////////////////////////////////
//		This Function is called every time a character key is pressed
///////////////////////////////////
void OnChar(HWND hwnd, UINT ch, int cRepeat)
	{

	}
///////////////////////////////////
//		This Function is called every time the Left Mouse Button is up
///////////////////////////////////
void OnLBU(HWND hwnd, int x, int y, UINT keyFlags)
	{
	if (x > 250)
		{
		PostQuitMessage(0);
		}

	}
///////////////////////////////////
//		This Function is called every time the Right Mouse Button is up
///////////////////////////////////
void OnRBU(HWND hwnd, int x, int y, UINT keyFlags)
	{


	}
///////////////////////////////////
//		This Function is called every time the Mouse Moves
///////////////////////////////////


void OnMM(HWND hwnd, int x, int y, UINT keyFlags)
{

	VsConstData.some_variable_a = (float)(x - 370) / 500.0;
	VsConstData.some_variable_b = (float)y / 500.0;
	VsConstData.some_variable_c = 0;
	VsConstData.some_variable_d = 0;

	if ((keyFlags & MK_LBUTTON) == MK_LBUTTON)
		{
		}

	if ((keyFlags & MK_RBUTTON) == MK_RBUTTON)
		{
		}

	// gives it a little buffer space
	if (lastMousePos < (x + 7) && lastMousePos > (x - 7))
	{
		cam.mouse = 0;
	}
	/* if last mouse movement is in a higher position 
	   than the mouse, turn the camera left */
	else if (lastMousePos > x)
	{
		cam.mouse = 1;
		lastMousePos = x;
	}
	// else turn the other way
	else if (lastMousePos < x)
	{
		cam.mouse = -1;
		lastMousePos = x;
	}
}

///////////////////////////////////
//		This Function is called once at the begin of a program
///////////////////////////////////
#define TIMER1 1

BOOL OnCreate(HWND hwnd, CREATESTRUCT FAR* lpCreateStruct)
	{
	hMain = hwnd;
	return TRUE;
	}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
	{
	HWND hwin;

	switch (id)
		{
			default: break;
		}

	}
//************************************************************************
void OnTimer(HWND hwnd, UINT id)
	{

	}
//************************************************************************
///////////////////////////////////
//		This Function is called every time the window has to be painted again
///////////////////////////////////


void OnPaint(HWND hwnd)
	{


	}
//****************************************************************************

//*************************************************************************
void OnKeyDown(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
	{
	switch (vk)
		{
			// scaling
			case 74:	// j
				toScale += 2;
				break;
			case 75:	// k
				toScale -= 2;
				break;

			// translate 3D object
			case 65:	// a
				toTranslateZ -= .2;
				break;
			case 68:	// d
				toTranslateZ += .2;
				break;
			case 83:	// s
				toTranslateY += .2;
				break;
			case 87:	// w
				toTranslateY -= .2;
				break;
		
			// rotate cam up/down
			case 81:	//q
				cam.qk = 1;
				break;
			case 69:	//e
				cam.ek = 1;
				break;

			// translate cam up/down/left/right
			case VK_LEFT:	// leftArrow
				cam.leftArrow = 1;
				break;
			case VK_RIGHT:	// rightArrow
				cam.rightArrow = 1;
				break;
			case VK_UP:	// upArrow
				cam.upArrow = 1;
				break;
			case VK_DOWN:	// downArrow
				cam.downArrow = 1;
				break;

			// change background
			case 0x20:	// spacebar
				change = !change;
				break;
			default: break;
		}
	}

//*************************************************************************
void OnKeyUp(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
	{
	switch (vk)
		{
			// rotate cam up/down
			case 81:	//q
				cam.qk = 0;
				break;
			case 69:	//e
				cam.ek = 0;
				break;

			// translate cam up/down/left/right
			case VK_LEFT:	// leftArrow
				cam.leftArrow = 0;
				break;
			case VK_RIGHT:	// rightArrow
				cam.rightArrow = 0;
				break;
			case VK_UP:	// up arrow
				cam.upArrow = 0;
				break;
			case VK_DOWN:	// down arrow
				cam.downArrow = 0;
				break;
			default: break;
		}
	}


//**************************************************************************
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{

	PAINTSTRUCT ps;
	HDC hdc;
	switch (message)
		{

		HANDLE_MSG(hwnd, WM_CHAR, OnChar);			// when a character key pressed 
		HANDLE_MSG(hwnd, WM_LBUTTONDOWN, OnLBD);	// when press left button
		HANDLE_MSG(hwnd, WM_LBUTTONUP, OnLBU);		// when release left button
		HANDLE_MSG(hwnd, WM_MOUSEMOVE, OnMM);		// when moving the mouse 
		HANDLE_MSG(hwnd, WM_KEYDOWN, OnKeyDown);	// when press key
		HANDLE_MSG(hwnd, WM_KEYUP, OnKeyUp);		// when release key
		HANDLE_MSG(hwnd, WM_CREATE, OnCreate);		
		HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);	
		HANDLE_MSG(hwnd, WM_TIMER, OnTimer);		
			case WM_PAINT:
				hdc = BeginPaint(hMain, &ps);
				EndPaint(hMain, &ps);
				break;
			case WM_ERASEBKGND:
				return (LRESULT)1;
			case WM_DESTROY:
				PostQuitMessage(0);
				break;
			default:
				return DefWindowProc(hwnd, message, wParam, lParam);
		}
	return 0;
	}

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DX11
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
	{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
	if (FAILED(hr))
		{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		if (pErrorBlob) pErrorBlob->Release();
		return hr;
		}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
	}