/* 
Copyright 2009-2013 Poznan Supercomputing and Networking Center

Authors:
Milosz Ciznicki miloszc@man.poznan.pl

GPU JPEG2K is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GPU JPEG2K is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with GPU JPEG2K. If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file cuda_errors.cu
 *
 * @author Jakub Misiorny <misiorny@man.poznan.pl>
 */

#include "cuda_errors.h"
#include <stdio.h>

void checkCUDAError(const char *msg)
{
    cudaError_t err = cudaGetLastError();
    if( cudaSuccess != err)
    {
        fprintf(stderr, "Cuda error: %s: %s.\n", msg, cudaGetErrorString( err) );
        exit(EXIT_FAILURE);
    }
}
