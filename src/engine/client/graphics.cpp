/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/detect.h>
#include <base/math.h>
#include <base/tl/threading.h>

#include <base/system.h>
#include <engine/external/pnglite/pnglite.h>

#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/keys.h>
#include <engine/console.h>

#include <math.h> // cosf, sinf
#include <string.h>
#include <malloc.h>

#include <gccore.h>

#include "graphics.h"

#define FIFO_SIZE (256 * 1024)
#define GL_MAX_TEXTURE_SIZE 512


static Mtx44 mtx_identity = {
	1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f
};


void CGraphics_Wii::Flush()
{
	if(m_NumVertices == 0)
		return;

	if(m_RenderEnable)
	{
		Mtx mat   = { 0 };
		mat[0][0] = 1; mat[0][3] = 0;
		mat[1][1] = 1; mat[1][3] = 0;

		GX_ClearVtxDesc();
		if (m_CurrTexture != -1)
		{
			GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
			GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
			GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

			GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ,  GX_F32,   0);
			GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
			GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST,   GX_F32,   0);

			GX_SetNumTexGens(1);
		
			GX_LoadTexMtxImm(mat, GX_TEXMTX0, GX_MTX2x4);
			GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);

			GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
			GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
		}
		else
		{
			GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
			GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

			GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ,  GX_F32,   0);
			GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

			GX_SetNumTexGens(0);

			GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

			GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
			GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
		}

		GX_Begin((g_Config.m_GfxQuadAsTriangle) ? GX_TRIANGLES : GX_QUADS, GX_VTXFMT0, m_NumVertices);

		for (int i=0; i<m_NumVertices; i++)
		{
			CVertex* v = &m_aVertices[i];

			GX_Position3f32(v->m_Pos.x, v->m_Pos.y, v->m_Pos.z);
			GX_Color4u8(v->m_Color.r*255, v->m_Color.g*255, v->m_Color.b*255, v->m_Color.a*255);
			if (m_CurrTexture != -1) GX_TexCoord2f32(v->m_Tex.u, v->m_Tex.v);
		}

		GX_End();
	}

	// Reset pointer
	m_NumVertices = 0;
}

void CGraphics_Wii::AddVertices(int Count)
{
	m_NumVertices += Count;
	if((m_NumVertices + Count) >= MAX_VERTICES)
		Flush();
}

void CGraphics_Wii::Rotate(const CPoint &rCenter, CVertex *pPoints, int NumPoints)
{
	float c = cosf(m_Rotation);
	float s = sinf(m_Rotation);
	float x, y;
	int i;

	for(i = 0; i < NumPoints; i++)
	{
		x = pPoints[i].m_Pos.x - rCenter.x;
		y = pPoints[i].m_Pos.y - rCenter.y;
		pPoints[i].m_Pos.x = x * c - y * s + rCenter.x;
		pPoints[i].m_Pos.y = x * s + y * c + rCenter.y;
	}
}

unsigned char CGraphics_Wii::Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp)
{
	int Value = 0;
	for(int x = 0; x < ScaleW; x++)
		for(int y = 0; y < ScaleH; y++)
			Value += pData[((v+y)*w+(u+x))*Bpp+Offset];
	return Value/(ScaleW*ScaleH);
}

unsigned char *CGraphics_Wii::Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData)
{
	unsigned char *pTmpData;
	int ScaleW = Width/NewWidth;
	int ScaleH = Height/NewHeight;

	int Bpp = 3;
	if(Format == CImageInfo::FORMAT_RGBA)
		Bpp = 4;

	pTmpData = (unsigned char *)mem_alloc(NewWidth*NewHeight*Bpp, 1);

	int c = 0;
	for(int y = 0; y < NewHeight; y++)
		for(int x = 0; x < NewWidth; x++, c++)
		{
			pTmpData[c*Bpp] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 0, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+1] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 1, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+2] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 2, ScaleW, ScaleH, Bpp);
			if(Bpp == 4)
				pTmpData[c*Bpp+3] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 3, ScaleW, ScaleH, Bpp);
		}

	return pTmpData;
}

CGraphics_Wii::CGraphics_Wii()
{
	m_NumVertices = 0;

	m_ScreenX0 = 0;
	m_ScreenY0 = 0;
	m_ScreenX1 = 0;
	m_ScreenY1 = 0;

	m_ScreenWidth = -1;
	m_ScreenHeight = -1;

	m_Rotation = 0;
	m_RotX = -1;
	m_RotY = -1;

	m_Drawing = 0;
	m_InvalidTexture = 0;
	m_CurrTexture = -1;

	m_TextureMemoryUsage = 0;

	m_RenderEnable = true;
	m_DoScreenshot = false;
}

void CGraphics_Wii::ClipEnable(int x, int y, int w, int h)
{
	if(x < 0)
		w += x;
	if(y < 0)
		h += y;

	x = clamp(x, 0, ScreenWidth());
	y = clamp(y, 0, ScreenHeight());
	w = clamp(w, 0, ScreenWidth()-x);
	h = clamp(h, 0, ScreenHeight()-y);

	GX_SetScissor(x, y, w, h);
}

void CGraphics_Wii::ClipDisable()
{
	GX_SetScissor(0, 0, mode->fbWidth, mode->efbHeight);
}

void CGraphics_Wii::BlendNone()
{
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
}

void CGraphics_Wii::BlendNormal()
{
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
}

void CGraphics_Wii::BlendAdditive()
{
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
}

void CGraphics_Wii::WrapNormal()
{
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void CGraphics_Wii::WrapClamp()
{
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

int CGraphics_Wii::MemoryUsage() const
{
	return m_TextureMemoryUsage;
}

void CGraphics_Wii::MapScreen(float TopLeftX, float TopLeftY, float BottomRightX, float BottomRightY)
{
	m_ScreenX0 = TopLeftX;
	m_ScreenY0 = TopLeftY;
	m_ScreenX1 = BottomRightX;
	m_ScreenY1 = BottomRightY;


	Mtx44 mat;
	mem_copy(&mat, &mtx_identity, sizeof(Mtx44));
	guOrtho(mat, TopLeftY, BottomRightY, TopLeftX, BottomRightX, 1.0f, 10.f);
	GX_LoadProjectionMtx(mat, GX_ORTHOGRAPHIC);
}

void CGraphics_Wii::GetScreen(float *pTopLeftX, float *pTopLeftY, float *pBottomRightX, float *pBottomRightY)
{
	*pTopLeftX = m_ScreenX0;
	*pTopLeftY = m_ScreenY0;
	*pBottomRightX = m_ScreenX1;
	*pBottomRightY = m_ScreenY1;
}

void CGraphics_Wii::LinesBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->LinesBegin twice");
	m_Drawing = DRAWING_LINES;
	SetColor(1,1,1,1);
}

void CGraphics_Wii::LinesEnd()
{
	dbg_assert(m_Drawing == DRAWING_LINES, "called Graphics()->LinesEnd without begin");
	Flush();
	m_Drawing = 0;
}

void CGraphics_Wii::LinesDraw(const CLineItem *pArray, int Num)
{
	dbg_assert(m_Drawing == DRAWING_LINES, "called Graphics()->LinesDraw without begin");

	for(int i = 0; i < Num; ++i)
	{
		m_aVertices[m_NumVertices + 2*i].m_Pos.x = pArray[i].m_X0;
		m_aVertices[m_NumVertices + 2*i].m_Pos.y = pArray[i].m_Y0;
		m_aVertices[m_NumVertices + 2*i].m_Tex = m_aTexture[0];
		m_aVertices[m_NumVertices + 2*i].m_Color = m_aColor[0];

		m_aVertices[m_NumVertices + 2*i + 1].m_Pos.x = pArray[i].m_X1;
		m_aVertices[m_NumVertices + 2*i + 1].m_Pos.y = pArray[i].m_Y1;
		m_aVertices[m_NumVertices + 2*i + 1].m_Tex = m_aTexture[1];
		m_aVertices[m_NumVertices + 2*i + 1].m_Color = m_aColor[1];
	}

	AddVertices(2*Num);
}

int CGraphics_Wii::UnloadTexture(int Index)
{
	if(Index == m_InvalidTexture)
		return 0;

	if(Index < 0)
		return 0;

	free(m_aTextures[Index].pixels);
	m_aTextures[Index].m_Next = m_FirstFreeTexture;
	m_TextureMemoryUsage -= m_aTextures[Index].m_MemSize;
	m_FirstFreeTexture = Index;
	return 0;
}

void CGraphics_Wii::ReorderPixels(CTexture* tex, const void* data, int srcWidth, int srcHeight, int OriginX, int OriginY, int RowWidth)
{
	int stride = GX_GetTexObjWidth(&tex->obj) * 4;
	OriginX &= ~0x03;
	OriginY &= ~0x03;

	// http://hitmen.c02.at/files/yagcd/yagcd/chap15.html
	//  section 15.35  TPL (Texture Palette)
	// "RGBA8 (4x4 tiles in two cache lines - first is AR and second is GB"
	const uint8_t *src = (const uint8_t*)data;
	uint8_t *dst = (uint8_t*)tex->pixels + stride * OriginY;
	
	for (int tileY = 0; tileY < srcHeight; tileY += 4)
	{
		for (int tileX = 0; tileX < srcWidth; tileX += 4) 
		{
			for (int y = 0; y < 4; y++) {
				for (int x = 0; x < 4; x++) {
					uint32_t idx = (((tileY + y) * RowWidth) + tileX + x) << 2;

					*dst++ = src[idx + 0]; // A
					*dst++ = src[idx + 1]; // R
				}
			}

			for (int y = 0; y < 4; y++) {
				for (int x = 0; x < 4; x++) {
					uint32_t idx = (((tileY + y) * RowWidth) + tileX + x) << 2;

					*dst++ = src[idx + 2]; // G
					*dst++ = src[idx + 3]; // B
				}
			}
		}
	}
}

static void ConvertTexture(u8* dst, const u8* src, int w, int h, int StoreFormat)
{
	int add = (StoreFormat == CImageInfo::FORMAT_ALPHA) ? 1 : (StoreFormat == CImageInfo::FORMAT_RGB) ? 3 : 4;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++, src += add, dst += 4)
		{
			if (StoreFormat == CImageInfo::FORMAT_ALPHA)
			{
				dst[0] = src[0];
				dst[1] = src[0];
				dst[2] = src[0];
				dst[3] = src[0];
			}
			else if (StoreFormat == CImageInfo::FORMAT_RGB)
			{
				dst[0] = 255;
				dst[1] = src[0];
				dst[2] = src[1];
				dst[3] = src[2];
			}
			else
			{
				dst[0] = src[3];
				dst[1] = src[0];
				dst[2] = src[1];
				dst[3] = src[2];
			}
		}
	}
}

int CGraphics_Wii::LoadTextureRawSub(int TextureID, int x, int y, int Width, int Height, int Format, const void *pData)
{
	return 0;
}

int CGraphics_Wii::LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags)
{
	u8* pTexData = (u8*)pData;
	u8* pTmpData = 0;
	int StoreGXformat = 0;
	int Tex = 0;

	// don't waste memory on texture if we are stress testing
	if(g_Config.m_DbgStress)
		return 	m_InvalidTexture;

	// grab texture
	Tex = m_FirstFreeTexture;
	m_FirstFreeTexture = m_aTextures[Tex].m_Next;
	m_aTextures[Tex].m_Next = -1;

	// resample if needed
	if(!(Flags&TEXLOAD_NORESAMPLE) && (Format == CImageInfo::FORMAT_RGBA || Format == CImageInfo::FORMAT_RGB))
	{
		if(Width > GL_MAX_TEXTURE_SIZE || Height > GL_MAX_TEXTURE_SIZE)
		{
			int NewWidth = min(Width, GL_MAX_TEXTURE_SIZE);
			float div = NewWidth/(float)Width;
			int NewHeight = Height * div;
			pTmpData = Rescale(Width, Height, NewWidth, NewHeight, Format, pTexData);
			pTexData = pTmpData;
			Width = NewWidth;
			Height = NewHeight;
		}
		else if(Width > 16 && Height > 16 && g_Config.m_GfxTextureQuality == 0)
		{
			pTmpData = Rescale(Width, Height, Width/2, Height/2, Format, pTexData);
			pTexData = pTmpData;
			Width /= 2;
			Height /= 2;
		}
	}

	// upload texture
	/*StoreGXformat = GL_RGBA;
	if(StoreFormat == CImageInfo::FORMAT_RGB)
		StoreGXformat = GL_RGB;
	else if(StoreFormat == CImageInfo::FORMAT_ALPHA)
		StoreGXformat = GL_ALPHA;
	*/

	u8* pFinalData = (u8*)mem_alloc(Width * Height * 4, 1);
	ConvertTexture(pFinalData, pTexData, Width, Height, StoreFormat);
	if (pTmpData) mem_free(pTmpData);

	int size = Width * Height * 4;
	m_aTextures[Tex].pixels = (u32*)memalign(32, 32 + size);

	GX_InitTexObj(&m_aTextures[Tex].obj, m_aTextures[Tex].pixels, Width, Height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
	//GX_InitTexObjFilterMode(m_aTextures[Tex].obj, GX_NEAR, GX_NEAR);

	ReorderPixels(&m_aTextures[Tex], pFinalData, Width, Height, 0, 0, Width);
	DCFlushRange(m_aTextures[Tex].pixels, size);
	mem_free(pFinalData);

	// calculate memory usage
	{
		int PixelSize = 4;
		/*if(StoreFormat == CImageInfo::FORMAT_RGB)
			PixelSize = 3;
		else if(StoreFormat == CImageInfo::FORMAT_ALPHA)
			PixelSize = 1;*/

		m_aTextures[Tex].m_MemSize = Width*Height*PixelSize;
	}

	m_TextureMemoryUsage += m_aTextures[Tex].m_MemSize;
	return Tex;
}

// simple uncompressed RGBA loaders
int CGraphics_Wii::LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags)
{
	int l = str_length(pFilename);
	int ID;
	CImageInfo Img;

	if(l < 3)
		return -1;
	if(LoadPNG(&Img, pFilename, StorageType))
	{
		if (StoreFormat == CImageInfo::FORMAT_AUTO)
			StoreFormat = Img.m_Format;

		ID = LoadTextureRaw(Img.m_Width, Img.m_Height, Img.m_Format, Img.m_pData, StoreFormat, Flags);
		mem_free(Img.m_pData);
		if(ID != m_InvalidTexture && g_Config.m_Debug)
			dbg_msg("graphics/texture", "loaded %s", pFilename);
		return ID;
	}

	return m_InvalidTexture;
}

int CGraphics_Wii::LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType)
{
	char aCompleteFilename[512];
	unsigned char *pBuffer;
	png_t Png; // ignore_convention

	// open file for reading
	png_init(0,0); // ignore_convention

	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType, aCompleteFilename, sizeof(aCompleteFilename));
	if(File)
		io_close(File);
	else
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
		return 0;
	}

	int Error = png_open_file(&Png, aCompleteFilename); // ignore_convention
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", aCompleteFilename);
		if(Error != PNG_FILE_ERROR)
			png_close_file(&Png); // ignore_convention
		return 0;
	}

	if(Png.depth != 8 || (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA)) // ignore_convention
	{
		dbg_msg("game/png", "invalid format. filename='%s'", aCompleteFilename);
		png_close_file(&Png); // ignore_convention
		return 0;
	}

	pBuffer = (unsigned char *)mem_alloc(Png.width * Png.height * Png.bpp, 1); // ignore_convention
	png_get_data(&Png, pBuffer); // ignore_convention
	png_close_file(&Png); // ignore_convention

	pImg->m_Width = Png.width; // ignore_convention
	pImg->m_Height = Png.height; // ignore_convention
	if(Png.color_type == PNG_TRUECOLOR) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGB;
	else if(Png.color_type == PNG_TRUECOLOR_ALPHA) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	pImg->m_pData = pBuffer;
	return 1;
}

void CGraphics_Wii::ScreenshotDirect(const char *pFilename)
{
	/*
	// fetch image data
	int y;
	int w = m_ScreenWidth;
	int h = m_ScreenHeight;
	unsigned char *pPixelData = (unsigned char *)mem_alloc(w*(h+1)*3, 1);
	unsigned char *pTempRow = pPixelData+w*h*3;
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0,0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// flip the pixel because opengl works from bottom left corner
	for(y = 0; y < h/2; y++)
	{
		mem_copy(pTempRow, pPixelData+y*w*3, w*3);
		mem_copy(pPixelData+y*w*3, pPixelData+(h-y-1)*w*3, w*3);
		mem_copy(pPixelData+(h-y-1)*w*3, pTempRow,w*3);
	}

	// find filename
	{
		char aWholePath[1024];
		png_t Png; // ignore_convention

		IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE, aWholePath, sizeof(aWholePath));
		if(File)
			io_close(File);

		// save png
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "saved screenshot to '%s'", aWholePath);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
		png_open_file_write(&Png, aWholePath); // ignore_convention
		png_set_data(&Png, w, h, 8, PNG_TRUECOLOR, (unsigned char *)pPixelData); // ignore_convention
		png_close_file(&Png); // ignore_convention
	}

	// clean up
	mem_free(pPixelData);
	*/
}

void CGraphics_Wii::TextureSet(int TextureID)
{
	dbg_assert(m_Drawing == 0, "called Graphics()->TextureSet within begin");
	m_CurrTexture = TextureID;

	if (TextureID != -1)
		GX_LoadTexObj(&(m_aTextures[TextureID].obj), GX_TEXMAP0);
}

void CGraphics_Wii::Clear(float r, float g, float b)
{
	GXColor col = {r*255, g*255, b*255, 255};
	GX_SetCopyClear(col, GX_MAX_Z24);
}

void CGraphics_Wii::QuadsBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->QuadsBegin twice");
	m_Drawing = DRAWING_QUADS;

	QuadsSetSubset(0,0,1,1);
	QuadsSetRotation(0);
	QuadsSetRotationCenter(-1, -1);
	SetColor(1,1,1,1);
}

void CGraphics_Wii::QuadsEnd()
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsEnd without begin");
	Flush();
	m_Drawing = 0;
}

void CGraphics_Wii::QuadsSetRotation(float Angle)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsSetRotation without begin");
	m_Rotation = Angle;
}

void CGraphics_Wii::QuadsSetRotationCenter(float X, float Y)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsSetRotation without begin");
	m_RotX = X;
	m_RotY = Y;
}

void CGraphics_Wii::SetColorVertex(const CColorVertex *pArray, int Num)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColorVertex without begin");

	for(int i = 0; i < Num; ++i)
	{
		m_aColor[pArray[i].m_Index].r = pArray[i].m_R;
		m_aColor[pArray[i].m_Index].g = pArray[i].m_G;
		m_aColor[pArray[i].m_Index].b = pArray[i].m_B;
		m_aColor[pArray[i].m_Index].a = pArray[i].m_A;
	}
}

void CGraphics_Wii::SetColor(float r, float g, float b, float a)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColor without begin");
	CColorVertex Array[4] = {
		CColorVertex(0, r, g, b, a),
		CColorVertex(1, r, g, b, a),
		CColorVertex(2, r, g, b, a),
		CColorVertex(3, r, g, b, a)};
	SetColorVertex(Array, 4);
}

void CGraphics_Wii::QuadsSetSubset(float TlU, float TlV, float BrU, float BrV)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsSetSubset without begin");

	m_aTexture[0].u = TlU;	m_aTexture[1].u = BrU;
	m_aTexture[0].v = TlV;	m_aTexture[1].v = TlV;

	m_aTexture[3].u = TlU;	m_aTexture[2].u = BrU;
	m_aTexture[3].v = BrV;	m_aTexture[2].v = BrV;
}

void CGraphics_Wii::QuadsSetSubsetFree(
	float x0, float y0, float x1, float y1,
	float x2, float y2, float x3, float y3)
{
	m_aTexture[0].u = x0; m_aTexture[0].v = y0;
	m_aTexture[1].u = x1; m_aTexture[1].v = y1;
	m_aTexture[2].u = x2; m_aTexture[2].v = y2;
	m_aTexture[3].u = x3; m_aTexture[3].v = y3;
}

void CGraphics_Wii::QuadsDraw(CQuadItem *pArray, int Num)
{
	for(int i = 0; i < Num; ++i)
	{
		pArray[i].m_X -= pArray[i].m_Width/2;
		pArray[i].m_Y -= pArray[i].m_Height/2;
	}

	QuadsDrawTL(pArray, Num);
}

void CGraphics_Wii::QuadsDrawTL(const CQuadItem *pArray, int Num)
{
	CPoint Center;
	Center.z = 0;

	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsDrawTL without begin");

	if(g_Config.m_GfxQuadAsTriangle)
	{
		for(int i = 0; i < Num; ++i)
		{
			// first triangle
			m_aVertices[m_NumVertices + 6*i].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 6*i].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 6*i].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 6*i].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 6*i + 1].m_Tex = m_aTexture[1];
			m_aVertices[m_NumVertices + 6*i + 1].m_Color = m_aColor[1];

			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 6*i + 2].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 6*i + 2].m_Color = m_aColor[2];

			// second triangle
			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 6*i + 3].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 6*i + 3].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 6*i + 4].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 6*i + 4].m_Color = m_aColor[2];

			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 6*i + 5].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 6*i + 5].m_Color = m_aColor[3];

			if(m_Rotation != 0)
			{
				Center.x = (m_RotX != -1) ? m_RotX : pArray[i].m_X + pArray[i].m_Width/2;
				Center.y = (m_RotY != -1) ? m_RotY : pArray[i].m_Y + pArray[i].m_Height/2;

				Rotate(Center, &m_aVertices[m_NumVertices + 6*i], 6);
			}
		}

		AddVertices(3*2*Num);
	}
	else
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 4*i].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 4*i].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 4*i].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 4*i].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 4*i + 1].m_Tex = m_aTexture[1];
			m_aVertices[m_NumVertices + 4*i + 1].m_Color = m_aColor[1];

			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 4*i + 2].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 4*i + 2].m_Color = m_aColor[2];

			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 4*i + 3].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 4*i + 3].m_Color = m_aColor[3];

			if(m_Rotation != 0)
			{
				Center.x = (m_RotX != -1) ? m_RotX : pArray[i].m_X + pArray[i].m_Width/2;
				Center.y = (m_RotY != -1) ? m_RotY : pArray[i].m_Y + pArray[i].m_Height/2;

				Rotate(Center, &m_aVertices[m_NumVertices + 4*i], 4);
			}
		}

		AddVertices(4*Num);
	}
}

void CGraphics_Wii::QuadsDrawFreeform(const CFreeformItem *pArray, int Num)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsDrawFreeform without begin");

	if(g_Config.m_GfxQuadAsTriangle)
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 6*i].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 6*i].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 6*i].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 6*i].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.x = pArray[i].m_X1;
			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.y = pArray[i].m_Y1;
			m_aVertices[m_NumVertices + 6*i + 1].m_Tex = m_aTexture[1];
			m_aVertices[m_NumVertices + 6*i + 1].m_Color = m_aColor[1];

			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 6*i + 2].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 6*i + 2].m_Color = m_aColor[3];

			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 6*i + 3].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 6*i + 3].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 6*i + 4].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 6*i + 4].m_Color = m_aColor[3];

			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.x = pArray[i].m_X2;
			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.y = pArray[i].m_Y2;
			m_aVertices[m_NumVertices + 6*i + 5].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 6*i + 5].m_Color = m_aColor[2];
		}

		AddVertices(3*2*Num);
	}
	else
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 4*i].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 4*i].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 4*i].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 4*i].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.x = pArray[i].m_X1;
			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.y = pArray[i].m_Y1;
			m_aVertices[m_NumVertices + 4*i + 1].m_Tex = m_aTexture[1];
			m_aVertices[m_NumVertices + 4*i + 1].m_Color = m_aColor[1];

			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 4*i + 2].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 4*i + 2].m_Color = m_aColor[3];

			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.x = pArray[i].m_X2;
			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.y = pArray[i].m_Y2;
			m_aVertices[m_NumVertices + 4*i + 3].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 4*i + 3].m_Color = m_aColor[2];
		}

		AddVertices(4*Num);
	}
}

void CGraphics_Wii::QuadsText(float x, float y, float Size, const char *pText)
{
	float StartX = x;

	while(*pText)
	{
		char c = *pText;
		pText++;

		if(c == '\n')
		{
			x = StartX;
			y += Size;
		}
		else
		{
			QuadsSetSubset(
				(c%16)/16.0f,
				(c/16)/16.0f,
				(c%16)/16.0f+1.0f/16.0f,
				(c/16)/16.0f+1.0f/16.0f);

			CQuadItem QuadItem(x, y, Size, Size);
			QuadsDrawTL(&QuadItem, 1);
			x += Size/2;
		}
	}
}

int CGraphics_Wii::Init()
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	// Set all z to -5.0f
	for(int i = 0; i < MAX_VERTICES; i++)
		m_aVertices[i].m_Pos.z = -5.0f;

	// init textures
	m_FirstFreeTexture = 0;
	for(int i = 0; i < MAX_TEXTURES; i++)
		m_aTextures[i].m_Next = i+1;
	m_aTextures[MAX_TEXTURES-1].m_Next = -1;

	// set some default settings
	/*
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glAlphaFunc(GL_GREATER, 0);
	glEnable(GL_ALPHA_TEST);
	glDepthMask(0);
	*/

	curFB = 0;
	// Initialise the video system
	VIDEO_Init();

	mode = VIDEO_GetPreferredMode(NULL);

	xfbs[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
	xfbs[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));

	// Set up the video registers with the chosen mode
	VIDEO_Configure(mode);
	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfbs[0]);

	// Make the display visible
	VIDEO_SetBlack(FALSE);
	// Flush the video register changes to the hardware
	VIDEO_Flush();
	// Wait for Video setup to complete
	VIDEO_WaitVSync();

	fifo_buffer = MEM_K0_TO_K1(memalign(32, FIFO_SIZE));
	memset(fifo_buffer, 0, FIFO_SIZE);

	GX_Init(fifo_buffer, FIFO_SIZE);
	GX_SetViewport(0, 0, mode->fbWidth, mode->efbHeight, 0, 1);
	GX_SetDispCopyYScale((f32)mode->xfbHeight / (f32)mode->efbHeight);
	GX_SetScissor(0, 0, mode->fbWidth, mode->efbHeight);
	GX_SetDispCopySrc(0, 0, mode->fbWidth, mode->efbHeight);
	GX_SetDispCopyDst(mode->fbWidth, mode->xfbHeight);
	GX_SetCopyFilter(mode->aa, mode->sample_pattern,
					 GX_TRUE, mode->vfilter);
	GX_SetFieldMode(mode->field_rendering,
					((mode->viHeight==2*mode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	GX_SetCullMode(GX_CULL_NONE);
	GX_SetZMode(0, 0, 0);
	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_InvVtxCache();
	
	GX_SetNumChans(1);



	m_ScreenWidth = mode->fbWidth;
	m_ScreenHeight = mode->efbHeight;
	g_Config.m_GfxScreenWidth = m_ScreenWidth;
	g_Config.m_GfxScreenHeight = m_ScreenHeight;


	// create null texture, will get id=0
	static const unsigned char aNullTextureData[] = {
		0xff,0x00,0x00,0xff, 0xff,0x00,0x00,0xff, 0x00,0xff,0x00,0xff, 0x00,0xff,0x00,0xff,
		0xff,0x00,0x00,0xff, 0xff,0x00,0x00,0xff, 0x00,0xff,0x00,0xff, 0x00,0xff,0x00,0xff,
		0x00,0x00,0xff,0xff, 0x00,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0x00,0xff,
		0x00,0x00,0xff,0xff, 0x00,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0x00,0xff,
	};

	m_InvalidTexture = LoadTextureRaw(4,4,CImageInfo::FORMAT_RGBA,aNullTextureData,CImageInfo::FORMAT_RGBA,TEXLOAD_NORESAMPLE);

	MapScreen(0,0,g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight);

	return 0;
}


void CGraphics_Wii::Shutdown()
{
	GX_AbortFrame();
	VIDEO_Flush();
}

void CGraphics_Wii::Minimize()
{
	
}

void CGraphics_Wii::Maximize()
{
	
}

int CGraphics_Wii::WindowActive()
{
	return 1;
}

int CGraphics_Wii::WindowOpen()
{
	return 1;
}

void CGraphics_Wii::NotifyWindow()
{
	
}

void CGraphics_Wii::TakeScreenshot(const char *pFilename)
{
	char aDate[20];
	str_timestamp(aDate, sizeof(aDate));
	str_format(m_aScreenshotName, sizeof(m_aScreenshotName), "screenshots/%s_%s.png", pFilename?pFilename:"screenshot", aDate);
	m_DoScreenshot = true;
}

void CGraphics_Wii::TakeCustomScreenshot(const char *pFilename)
{
	str_copy(m_aScreenshotName, pFilename, sizeof(m_aScreenshotName));
	m_DoScreenshot = true;
}


void CGraphics_Wii::Swap()
{
	if(m_DoScreenshot)
	{
		if(WindowActive())
			ScreenshotDirect(m_aScreenshotName);
		m_DoScreenshot = false;
	}

	curFB ^= 1; // swap "front" and "back" buffers
	GX_CopyDisp(xfbs[curFB], GX_TRUE);
	GX_DrawDone();
	
	VIDEO_SetNextFramebuffer(xfbs[curFB]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}


int CGraphics_Wii::GetVideoModes(CVideoMode *pModes, int MaxModes)
{
	pModes[0].m_Width = mode->fbWidth;
	pModes[0].m_Height = mode->efbHeight;
	pModes[0].m_Red = 8;
	pModes[0].m_Green = 8;
	pModes[0].m_Blue = 8;
	return 1;
}

// syncronization
void CGraphics_Wii::InsertSignal(semaphore *pSemaphore)
{
	pSemaphore->signal();
}

bool CGraphics_Wii::IsIdle()
{
	return true;
}

void CGraphics_Wii::WaitForIdle()
{
}

extern IEngineGraphics *CreateEngineGraphics() { return new CGraphics_Wii(); }
