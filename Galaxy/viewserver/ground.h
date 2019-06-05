#define WIN32_LEAN_AND_MEAN  
#include <windows.h>
#include <windowsx.h>
#include <malloc.h>
#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <time.h>
#include <stdlib.h>
#include <sys/timeb.h>
#include <fstream>
using namespace std;
#include <direct.h>
#include <commdlg.h>
#include <malloc.h>
#include <cmath>
#include <string.h>
#include <tchar.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>


struct SimpleVertex
	{
	XMFLOAT3 Pos;//12 byte
	XMFLOAT2 Tex;//8 byte
	XMFLOAT3 Norm;//12 byte
	};

struct SimpleVertexRGB
{
	XMFLOAT3 Pos; // 12 byte
	XMFLOAT4 Col; // 16 byte
	XMFLOAT3 Norm; // 12 byte
};


// loads a 3DS file into a vertex buffer:
bool Load3DS(char *filename, ID3D11Device* g_pd3dDevice, ID3D11Buffer **ppVertexBuffer, int *vertex_count,bool gouraurd);
// loads an OBJ file into a vertex buffer:
bool LoadOBJ(char * filename, ID3D11Device * g_pd3dDevice, ID3D11Buffer ** ppVertexBuffer, int * vertex_count);

//for matrix and vector operations, some handy functions:
float length(const XMFLOAT3 &v);
XMFLOAT3 mul(const XMMATRIX &M, const XMFLOAT3 &rhs);
XMMATRIX mul(const XMMATRIX &lhs, const XMMATRIX &rhs);
float dot(XMFLOAT3 a, XMFLOAT3 b);
XMFLOAT3 cross(XMFLOAT3 a, XMFLOAT3 b);
XMFLOAT3 normalize(const  XMFLOAT3 &a);
XMFLOAT3 operator+(const XMFLOAT3 lhs, const XMFLOAT3 rhs);
XMFLOAT2 operator+(const XMFLOAT2 lhs, const XMFLOAT2 rhs);
XMFLOAT3 operator-(const XMFLOAT3 lhs, const XMFLOAT3 rhs);

class camera
	{
	public:
		float angle_y,
			  angle_x,
			  angle_z;

		XMFLOAT3 pos;
		int ak, downArrow, dk, upArrow, leftArrow, rightArrow, mouse;//stores if a key is pressed
		int ek, qk;
		camera()
		{
			angle_y = 0;
			angle_x = 0;
			angle_z;
			pos = XMFLOAT3(-7, 3, 44);
			ek = qk = ak = downArrow = dk = upArrow = leftArrow = rightArrow = mouse = 0;//0 not pressed, 1 pressed
		}
		XMMATRIX calculate_view(XMMATRIX &V)
		{
			//calculate the matrices Ry, Tz
			XMMATRIX Ry, Rx, Rz, T;
			
			float anglespeed = 0.0006;
			float speed = 0.003;

			//rotation

			// set mouse movement
			if (mouse == 0)
			{
				angle_y = angle_y;
			}
			else if (mouse == 1)
				{
				angle_y = angle_y + anglespeed;
				}
			else if (mouse == -1)
				{
				angle_y = angle_y - anglespeed;
				}

			if (qk == 1)
				{
				angle_x = angle_x + anglespeed;
				}
			else if (ek == 1)
				{
				angle_x = angle_x - anglespeed;
				}
			Ry = XMMatrixRotationY(angle_y);
			Rx = XMMatrixRotationX(angle_x);
			Rz = XMMatrixRotationZ(angle_z);

			//translation

			XMFLOAT3 lookat_worldcoord = XMFLOAT3(0, 0, speed);
			XMFLOAT3 lookat_worldcoord2 = XMFLOAT3(speed, 0, 0);
			XMMATRIX R = Rx*Ry;
			XMMATRIX R2 = Ry * Rz;
			XMFLOAT3 lookat_viewcoord = mul(R, lookat_worldcoord);
			XMFLOAT3 lookat_viewcoord2 = mul(R2, lookat_worldcoord2);

			if (upArrow == 1)
			{
				pos = pos - lookat_viewcoord;
					}
			else if (downArrow == 1)
			{
				pos = pos + lookat_viewcoord;
			}
			if (leftArrow == 1)
			{
				pos = pos - lookat_viewcoord2;
			}
			else if (rightArrow == 1)
			{
				pos = pos + lookat_viewcoord2;
			}

			// scaling



			T = XMMatrixTranslation(-pos.x, -pos.y, pos.z);
			
			return T*V*Ry*Rz*Rx;
		}


	};