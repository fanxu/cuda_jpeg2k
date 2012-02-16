#pragma once

#include <cuda_runtime.h>
#include <cuda_runtime_api.h>

#include "../../include/memory.h"

#define THREADS 32

#ifdef __CDT_PARSER__
#define __global__
#define __device__
#define __constant__
#define __shared__
#endif

#define MAX_MAG_BITS 20

typedef struct
{
	int length;
	unsigned char significantBits;
	unsigned char codingPasses;
	unsigned char width;
	unsigned char nominalWidth;
	unsigned char height;
	unsigned char stripeNo;
	unsigned char magbits;
	unsigned char subband;
	unsigned char compType;
	unsigned char dwtLevel;
	float stepSize;

	int magconOffset;

	int* coefficients;
} CodeBlockAdditionalInfo;

namespace GPU_JPEG2K
{
	typedef unsigned int CoefficientState;
	typedef unsigned char byte;
	
	typedef struct
	{
		float dist;
		float slope;
		int L;
		int feasiblePoint;
	} PcrdCodeblock;

	typedef struct
	{
		int feasibleNum;
		int Lfirst;
	//	int nStates;
	} PcrdCodeblockInfo;

	void _launch_encode_pcrd(dim3 gridDim, dim3 blockDim, CoefficientState *coeffBuffors, byte *outbuf, int maxThreadBufforLength, CodeBlockAdditionalInfo *infos, int codeBlocks, const int maxMQStatesPerCodeBlock, PcrdCodeblock *pcrdCodeblocks, PcrdCodeblockInfo *pcrdCodeblockInfos, mem_mg_t *mem_mg);
	void launch_encode(dim3 gridDim, dim3 blockDim, CoefficientState *coeffBuffors, byte *outbuf, int maxThreadBufforLength, CodeBlockAdditionalInfo *infos, int codeBlocks, mem_mg_t *mem_mg);
	void launch_decode(dim3 gridDim, dim3 blockDim, CoefficientState *coeffBuffors, byte *inbuf, int maxThreadBufforLength, CodeBlockAdditionalInfo *infos, int codeBlocks);
}
