#include <vector>								// STL vector<>
#include <assert.h>							// assert()
#include <windows.h>						// timeGetTime()
#include "WorkQueue.h"					// CWorkQueue
#include <math.h>								// log()

typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;

// higher value can result in longer computation time
//const uint32 max_iter=256;
const uint32 max_iter=255;


// 1 byte per pixel
// RAW - can be loaded by Photoshop
bool WriteBitmap( const char *szFilename, const uint8 *pData, const uint32 dwWidth, const uint32 dwHeight )
{
	assert(szFilename);
	assert(pData);

	FILE *out = fopen(szFilename,"wb");

	if(!out)
		return false;

	fwrite(pData,dwWidth*dwHeight,1,out);

	fclose(out);
	return true;
}

#define FLT			double

// julia fractal
uint32 ComputeFractAt( FLT fX, FLT fY )
{
	uint32 iter=0;

	FLT dist2=0;
	const FLT maxdist2=2.0f*2.0f * 400.0f;

	// defines which julia fractal we want to compute
//	const float fCx=-0.8f, fCy=0.2f;							// pretty but higher iteration counts might not lead to much slower computations
//	const float fCx=-0.75f, fCy=0.18f;							// pretty but higher iteration counts might not lead to much slower computations
//	const float fCx=-0.73f, fCy=0.176f;							// good for performance measurements
	const FLT fCx=-0.74543f, fCy=0.11301f;							// good for performance measurements

	while(dist2<=maxdist2 && iter<max_iter)
	{
		FLT fX2 = fX*fX-fY*fY+fCx;
		FLT fY2 = 2*fX*fY+fCy;

		fX=fX2;fY=fY2;

		++iter;
		dist2=fX*fX+fY*fY;
	}

	return iter;
}



void ComputeBlock( uint8 *pOutMem, const uint32 dwWidth, const uint32 dwHeight,
	const uint32 dwMinX, const uint32 dwMinY, const uint32 dwSubWidth, const uint32 dwSubHeight,
	const float fMinX, const float fMinY, const float fMaxX, const float fMaxY )
{
	assert(pOutMem);

	FLT fStepX = ((FLT)fMaxX-(FLT)fMinX)/dwSubWidth;
	FLT fStepY = ((FLT)fMaxY-(FLT)fMinY)/dwSubHeight;

	for(uint32 dwY=0;dwY<dwSubHeight;++dwY)
	{
		FLT fY = (FLT)fMinY + dwY*fStepY;
		uint8 *pDst = &pOutMem[(dwMinY+dwY)*dwWidth+dwMinX];

		for(uint32 dwX=0;dwX<dwSubWidth;++dwX)
		{
			FLT fX = (FLT)fMinX + dwX*fStepX;

			uint32 dwInt = ComputeFractAt(fX,fY);

			double bias = 200;

//			*pDst++ = (dwInt*7)&0xff;
//			*pDst++ = dwInt==max_iter ? 0 : 0xff;
//			*pDst++ = 255-(uint8)min(255.0f,max(0.0f,(255.0+bias)*log((double)dwInt)/log((double)max_iter)-bias));
			*pDst++ = dwInt & 0xff;
		}
	}
}


class CThreadWorkElement :public WorkItemBase
{
public:
	uint8 *pOutMem;
	uint32 dwWidth;
	uint32 dwHeight;
	uint32 dwMinX;
	uint32 dwMinY;
	uint32 dwSubWidth;
	uint32 dwSubHeight;
	float fMinX;
	float fMinY;
	float fMaxX;
	float fMaxY;

	void Debug() const
	{
		char str[256];

		sprintf(str,"Debug %d %d\n",dwMinX/dwSubWidth,dwMinY/dwSubHeight);
		OutputDebugStringA(str);
	}

  virtual void DoWork(void* pThreadContext)
	{
/*		char str[256];

		sprintf(str,"DoWork %d\n",(int)pThreadContext);
		OutputDebugStringA(str);
*/

//		Debug();
		ComputeBlock(pOutMem,dwWidth,dwHeight,dwMinX,dwMinY,dwSubWidth,dwSubHeight,fMinX,fMinY,fMaxX,fMaxY);
	}

  virtual void Abort()
	{
		OutputDebugStringA("Abort\n");
	}
};


int main()
{
	printf("processing ....\n");

	uint32 dwTime1=timeGetTime();

//	const uint32 dwWidth=2048*4; 
//	const uint32 dwHeight=2048*4;
	const uint32 dwSubWidth=128;
	const uint32 dwSubHeight=128;

	const uint32 dwWidth = dwSubWidth	*16	* 4; 
	const uint32 dwHeight = dwSubHeight	*10 * 4;

	std::vector<uint8> Bitmap;

	Bitmap.resize(dwWidth*dwHeight,0);

	// rectangle to zoom into interesting areas
//	float fMinX=-1.5f, fMinY=-1.5f, fMaxX=1.5f, fMaxY=1.5f;
	float fMinX=-1.6f, fMinY=-1.0f, fMaxX=1.6f, fMaxY=1.0f;
//	float fMinX=-1.0f, fMinY=-1.0f, fMaxX=1.0f, fMaxY=1.0f;			// zoom in - more computations needed

	fMinX*=1.2f;
	fMinY*=1.2f;
	fMaxX*=1.2f;
	fMaxY*=1.2f;

	float fScaleX = fMaxX-fMinX;
	float fScaleY = fMaxY-fMinY;



/*
	for(uint32 dwY=0;dwY<dwHeight;dwY+=dwSubHeight)
	for(uint32 dwX=0;dwX<dwWidth;dwX+=dwSubWidth)
	{
		float fSubMinX = fMinX + fScaleX * (float)dwX/(float)dwWidth;
		float fSubMinY = fMinY + fScaleY * (float)dwY/(float)dwHeight;
		float fSubMaxX = fMinX + fScaleX * (float)(dwX+dwSubWidth)/(float)dwWidth;
		float fSubMaxY = fMinY + fScaleY * (float)(dwY+dwSubHeight)/(float)dwHeight;

		ComputeBlock(&Bitmap[0],dwWidth,dwHeight,dwX,dwY,dwSubWidth,dwSubHeight,fSubMinX,fSubMinY,fSubMaxX,fSubMaxY);
	}
*/

	
	{
		CWorkQueue queue;

		void *pThreadNo[] = { (void *)1, (void *)2};

		queue.Create(1,(void **)&pThreadNo);		// number of additional threads


		for(uint32 dwY=0;dwY<dwHeight;dwY+=dwSubHeight)
		for(uint32 dwX=0;dwX<dwWidth;dwX+=dwSubWidth)
		{
			float fSubMinX = fMinX + fScaleX * (float)dwX/(float)dwWidth;
			float fSubMinY = fMinY + fScaleY * (float)dwY/(float)dwHeight;
			float fSubMaxX = fMinX + fScaleX * (float)(dwX+dwSubWidth)/(float)dwWidth;
			float fSubMaxY = fMinY + fScaleY * (float)(dwY+dwSubHeight)/(float)dwHeight;

			CThreadWorkElement *pEl = new CThreadWorkElement;

			pEl->pOutMem=&Bitmap[0];
			pEl->dwWidth=dwWidth;
			pEl->dwHeight=dwHeight;
			pEl->dwMinX=dwX;
			pEl->dwMinY=dwY;
			pEl->dwSubWidth=dwSubWidth;
			pEl->dwSubHeight=dwSubHeight;
			pEl->fMinX=fSubMinX;
			pEl->fMinY=fSubMinY;
			pEl->fMaxX=fSubMaxX;
			pEl->fMaxY=fSubMaxY;

//			pEl->Debug();

			queue.InsertWorkItem(pEl);
		}

		queue.WaitTillAllFinished();
	}









	uint32 dwTime2=timeGetTime();



	WriteBitmap("out.raw",&Bitmap[0],dwWidth,dwHeight);

	printf("\n      %dx%d i:%d ... %d ms\n",dwWidth,dwHeight, max_iter, dwTime2-dwTime1);
	
	// 31188 ST 1 thread, release, AMDx2, 22375 fp fast
	// 15875,15781,15766,15797,15938,15969,15938 MT 1 thread, release, AMDx2    158xx fp precise, 209xx fp strict, 100xx fast
	// 15938 MT 2 threads, release, AMDx2

	getchar();

	return 0;
}

