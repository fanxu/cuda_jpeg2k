/*
 * mqc_wrapper.cpp
 *
 *  Created on: Dec 9, 2011
 *      Author: miloszc
 */

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "mqc_wrapper.h"
#include "mqc_common.h"
#include "../gpu_coeff_coder2.cuh"
#include "mqc_develop.h"
extern "C" {
#include "timer.h"
#include "mqc_data.h"
#include "../../../misc/memory_management.cuh"
}

static unsigned char* mqc_gpu_encode_(EntropyCodingTaskInfo *infos, CodeBlockAdditionalInfo* mqc_data, int codeBlocks,
		unsigned char* d_cxds, int maxOutLength, mem_mg_t *mem_mg, const char* param = 0, const char* name_suffix = 0) {
	// Initialize CUDA
	cudaError cuerr = cudaSuccess;

	// Determine cxd blocks count
	int cxd_block_count = codeBlocks;
	// Allocate CPU memory for cxd blocks
//	struct cxd_block* cxd_blocks = new struct cxd_block[cxd_block_count];
	struct cxd_block* cxd_blocks = (struct cxd_block *)mem_mg->alloc->host(sizeof(struct cxd_block) * cxd_block_count, mem_mg->ctx);
	// Fill CPU memory with cxd blocks
	int cxd_index = 0;
	int byte_index = 0;
	int cxd_block_index = 0;
	for (int cblk_index = 0; cblk_index < codeBlocks; cblk_index++) {
		cxd_blocks[cxd_block_index].cxd_begin = cxd_index;
		cxd_blocks[cxd_block_index].cxd_count = mqc_data[cblk_index].length > 0 ? mqc_data[cblk_index].length : 0;
		cxd_blocks[cxd_block_index].byte_begin = byte_index;
		cxd_blocks[cxd_block_index].byte_count = cxd_get_buffer_size(cxd_blocks[cxd_block_index].cxd_count);

		cxd_index += /*cxd_blocks[cxd_block_index].cxd_count*/maxOutLength;
		byte_index += cxd_blocks[cxd_block_index].byte_count;
		cxd_block_index++;
	}

	// Allocate GPU memory for cxd blocks
	struct cxd_block* d_cxd_blocks;
	d_cxd_blocks = (struct cxd_block *)mem_mg->alloc->dev(cxd_block_count * sizeof(struct cxd_block), mem_mg->ctx);

	// Fill GPU memory with cxd blocks
	cuda_memcpy_htd((void*) cxd_blocks, (void*) d_cxd_blocks, cxd_block_count * sizeof(struct cxd_block));

	// Allocate GPU memory for output bytes
	unsigned char* d_bytes = (unsigned char *)mem_mg->alloc->dev(1 + byte_index * sizeof(unsigned char), mem_mg->ctx);

	// Zero memory and move pointer by one (encoder access bp-1 so we must have that position)
	cuerr = cudaMemset((void*) d_bytes, 0, 1 + byte_index * sizeof(unsigned char));
	if (cuerr != cudaSuccess) {
		std::cout << "Can't memset for output bytes: " << cudaGetErrorString(cuerr) << std::endl;
		return NULL;
	}
	d_bytes++;

	// Init encoder
//    mqc_gpu_init(param);
	mqc_gpu_develop_init(param);

	// Encode on GPU
//        mqc_gpu_encode(d_cxd_blocks,cxd_block_count,d_cxds,d_bytes);
	mqc_gpu_develop_encode(d_cxd_blocks, cxd_block_count, d_cxds, d_bytes);

//		printf("mqc %f\n", duration[run_index]);

	// TODO: Check correction for carry bit
	// It seems like it is not needed but who nows, check it for sure

	// Allocate CPU memory for output bytes
//		unsigned char* bytes = new unsigned char[byte_index];
	unsigned char* bytes = (unsigned char *)mem_mg->alloc->host(sizeof(unsigned char) * byte_index, mem_mg->ctx);
	// Copy output bytes to CPU memory
	cuda_memcpy_dth(d_bytes, bytes, byte_index * sizeof(unsigned char));
	// Copy cxd blocks to CPU memory
	cuda_memcpy_dth(d_cxd_blocks, cxd_blocks, cxd_block_count * sizeof(struct cxd_block));

	// Check output bytes
	int cblk_index;
	for (cblk_index = 0; cblk_index < codeBlocks; cblk_index++) {
		EntropyCodingTaskInfo * cblk = &infos[cblk_index];
		struct cxd_block* cxd_block = &cxd_blocks[cblk_index];
		cblk->length = cxd_block->byte_count > 0 ? cxd_block->byte_count : 0;
//		cblk->codeStream = (unsigned char *)mem_mg->alloc->host(sizeof(unsigned char) * cxd_block->byte_count, mem_mg->ctx);
		cblk->codeStream = &bytes[cxd_block->byte_begin];
//		cuda_memcpy_hth(&bytes[cxd_block->byte_begin], cblk->codeStream, sizeof(unsigned char) * cxd_block->byte_count);
	}

	// Free CPU Memory
//		delete[] bytes;
//	mem_mg->dealloc->host(bytes, mem_mg->ctx);

	// Deinit encoder
	//    mqc_gpu_deinit();
	mqc_gpu_develop_deinit();

	// Free GPU memory
	mem_mg->dealloc->dev((void*) --d_bytes, mem_mg->ctx);
	mem_mg->dealloc->dev((void*) d_cxd_blocks, mem_mg->ctx);

	// Free CPU memory
//	delete[] cxd_blocks;
	mem_mg->dealloc->host(cxd_blocks, mem_mg->ctx);

	return bytes;
}

void mqc_gpu_encode_test() {
	char file_name[128];
	sprintf(file_name, "/home/miloszc/Projects/images/rgb8bit/flower_foveon.ppm\0");
	struct mqc_data* mqc_data = mqc_data_create_from_image(file_name);
	if(mqc_data == 0) {
		std::cerr << "Can't receive data from openjpeg: " << std::endl;
		return;
	}

	int maxOutLength = (4096 * 10);

    // Initialize CUDA
    cudaError cuerr = cudaSuccess;

    // Determine cxd count
    int cxd_count = 0;
    int byte_index = 0;
    for ( int cblk_index = 0; cblk_index < mqc_data->cblk_count; cblk_index++ ) {
    	struct mqc_data_cblk *cblk = mqc_data->cblks[cblk_index];
    	cxd_count += cblk->cxd_count;
    	byte_index += cblk->byte_count;
    }

    // Allocate CPU memory for CX,D pairs
//    int cxd_size = cxd_array_size(cxd_count);
    int cxd_size = mqc_data->cblk_count * maxOutLength;
    unsigned char* cxds = new unsigned char[cxd_size];
    memset(cxds,0,cxd_size);
    // Fill CPU memory with CX,D pairs

    for ( int cblk_index = 0; cblk_index < mqc_data->cblk_count; cblk_index++ ) {
        struct mqc_data_cblk* cblk = mqc_data->cblks[cblk_index];
        int index = cblk_index * maxOutLength;
        for ( int cxd_index = 0; cxd_index < cblk->cxd_count; cxd_index++ ) {
            cxd_array_put(cxds, index, cblk->cxds[cxd_index].cx, cblk->cxds[cxd_index].d);
            index++;
        }
    }

    // Allocate GPU memory for CX,D pairs
    unsigned char* d_cxds;
    cuerr = cudaMalloc((void**)&d_cxds, cxd_size * sizeof(unsigned char));
    if ( cuerr != cudaSuccess ) {
        std::cerr << "Can't allocate device memory for cxd pairs: " << cudaGetErrorString(cuerr) << std::endl;
        return;
    }
    // Fill GPU memory with CX,D pairs
    cudaMemcpy((void*)d_cxds, (void*)cxds, cxd_size * sizeof(unsigned char), cudaMemcpyHostToDevice);

    int codeBlocks = mqc_data->cblk_count;
    CodeBlockAdditionalInfo *h_infos = (CodeBlockAdditionalInfo *) malloc(sizeof(CodeBlockAdditionalInfo) * codeBlocks);

    for(int i = 0; i < codeBlocks; ++i) {
    	struct mqc_data_cblk* cblk = mqc_data->cblks[i];
    	h_infos[i].length = cblk->cxd_count;
    }

    EntropyCodingTaskInfo *infos = (EntropyCodingTaskInfo *) malloc(sizeof(EntropyCodingTaskInfo) * codeBlocks);

    mem_mg_t *mem_mg;
	mqc_gpu_encode_(infos, h_infos, codeBlocks, d_cxds, maxOutLength, mem_mg);

	// Check output bytes
	int cblk_index;
	int byte_count = 0;
	bool correct = true;
	for ( cblk_index = 0; cblk_index < mqc_data->cblk_count; cblk_index++ ) {
		struct mqc_data_cblk* cblk = mqc_data->cblks[cblk_index];
		EntropyCodingTaskInfo* cxd_block = &infos[cblk_index];
		byte_count += cxd_block->length;
		if ( cblk->byte_count != cxd_block->length ) {
			correct = false;
			std::cerr << "WRONG at block [" << cblk_index << "], because byte count [";
			std::cerr << cxd_block->length << "] != [" << cblk->byte_count << "]";
			break;
		} else {
			for ( int byte_index = 0; byte_index < cblk->byte_count; byte_index++ ) {
				if ( cxd_block->codeStream[byte_index] != cblk->bytes[byte_index] ) {
					correct = false;
					std::cerr << "WRONG at block [" << cblk_index << "] at byte [" << byte_index << "]";
					std::cerr << " because [" << (int)cxd_block->codeStream[byte_index];
					std::cerr << "] != [" << (int)cblk->bytes[byte_index] << "]";
					break;
				}
			}
			if ( correct == false )
				break;
		}
	}

	delete[] cxds;
	cudaFree((void *)d_cxds);
}

unsigned char* mqc_gpu_encode(EntropyCodingTaskInfo *infos, CodeBlockAdditionalInfo* h_infos, int codeBlocks,
		unsigned char *d_cxd_pairs, int maxOutLength, mem_mg_t *mem_mg) {
// Initialize CUDA
	cudaError cuerr = cudaSuccess;

/*	int cxd_count = 0;

	for (int i = 0; i < codeBlocks; ++i) {
		cxd_count += h_infos[i].length > 0 ? h_infos[i].length : 0;
//		if(i < 1000)
//			printf("%d) %d\n", i, h_infos[i].length);
	}

// Allocate GPU memory for CX,D pairs
	unsigned char* d_cxds;
	cuerr = cudaMalloc((void**) &d_cxds, cxd_count * sizeof(unsigned char));
	if (cuerr != cudaSuccess) {
		std::cerr << "Can't allocate device memory for cxd pairs: " << cudaGetErrorString(cuerr) << std::endl;
		return;
	}

	int cxd_idx = 0;
	for (int i = 0; i < codeBlocks; i++) {
		if (h_infos[i].length > 0) {
			int len = h_infos[i].length;
//			printf("%d) %d\n", i, len);
			cuerr = cudaMemcpy((void*) (d_cxds + cxd_idx), (void*) (d_cxd_pairs + i * maxOutLength), len * sizeof(unsigned char),
					cudaMemcpyDeviceToDevice);
			if (cuerr != cudaSuccess) {
				std::cerr << "Can't copy device memory for input CX,D pairs: " << cudaGetErrorString(cuerr)
						<< std::endl;
				return;
			}
			cxd_idx += h_infos[i].length;
		}
	}*/

	return mqc_gpu_encode_(infos, h_infos, codeBlocks, /*d_cxds*/d_cxd_pairs, maxOutLength, mem_mg);
//	mqc_gpu_encode_test();

//	cudaFree((void *)d_cxds);
}
