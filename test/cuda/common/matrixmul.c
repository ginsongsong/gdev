#include <cuda.h>
#ifdef __KERNEL__ /* just for measurement */
#define printf printk
#define malloc vmalloc
#define free vfree
#define gettimeofday(x, y) do_gettimeofday(x)
#include <linux/slab.h>
#include <linux/time.h>
#else /* just for measurement */
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#endif

int cuda_test_matrixmul(unsigned int n)
{
	int i, j, idx;
	CUresult res;
	CUdevice dev;
	CUcontext ctx;
	CUfunction function;
	CUmodule module;
	CUdeviceptr a_dev, b_dev, c_dev;
	unsigned int *a = (unsigned int *) malloc (n*n * sizeof(unsigned int));
	unsigned int *b = (unsigned int *) malloc (n*n * sizeof(unsigned int));
	unsigned int *c = (unsigned int *) malloc (n*n * sizeof(unsigned int));

	res = cuInit(0);
	if (res != CUDA_SUCCESS) {
		printf("cuInit failed: res = %u\n", res);
		return -1;
	}

	res = cuDeviceGet(&dev, 0);
	if (res != CUDA_SUCCESS) {
		printf("cuDeviceGet failed: res = %u\n", res);
		return -1;
	}

	res = cuCtxCreate(&ctx, 0, dev);
	if (res != CUDA_SUCCESS) {
		printf("cuCtxCreate failed: res = %u\n", res);
		return -1;
	}

	res = cuModuleLoad(&module, "./matrixmul_gpu.cubin");
	if (res != CUDA_SUCCESS) {
		printf("cuModuleLoad() failed\n");
		return -1;
	}
	res = cuModuleGetFunction(&function, module, "_Z8multiplyPjS_S_i");
	if (res != CUDA_SUCCESS) {
		printf("cuModuleGetFunction() failed\n");
		return -1;
	}
	res = cuFuncSetSharedSize(function, 0x40); /* just random */
	if (res != CUDA_SUCCESS) {
		printf("cuFuncSetSharedSize() failed\n");
		return -1;
	}
	res = cuFuncSetBlockShape(function, n, 1, 1);
	if (res != CUDA_SUCCESS) {
		printf("cuFuncSetBlockShape() failed\n");
		return -1;
	}

	/* a[] */
	res = cuMemAlloc(&a_dev, n*n * sizeof(unsigned int));
	if (res != CUDA_SUCCESS) {
		printf("cuMemAlloc (a) failed\n");
		return -1;
	}
	/* b[] */
	res = cuMemAlloc(&b_dev, n*n * sizeof(unsigned int));
	if (res != CUDA_SUCCESS) {
		printf("cuMemAlloc (b) failed\n");
		return -1;
	}
	/* c[] */
	res = cuMemAlloc(&c_dev, n*n * sizeof(unsigned int));
	if (res != CUDA_SUCCESS) {
		printf("cuMemAlloc (c) failed\n");
		return -1;
	}

	/* upload a[] and b[] */
	res = cuMemcpyHtoD(a_dev, a, n*n * sizeof(unsigned int));
	if (res != CUDA_SUCCESS) {
		printf("cuMemcpyHtoD (a) failed: res = %u\n", res);
		return -1;
	}
	res = cuMemcpyHtoD(b_dev, b, n*n * sizeof(unsigned int));
	if (res != CUDA_SUCCESS) {
		printf("cuMemcpyHtoD (b) failed: res = %u\n", res);
		return -1;
	}

	/* set kernel parameters */
	res = cuParamSeti(function, 0, a_dev);	
	if (res != CUDA_SUCCESS) {
		printf("cuParamSeti (a) failed: res = %u\n", res);
		return -1;
	}
	res = cuParamSeti(function, 4, a_dev >> 32);
	if (res != CUDA_SUCCESS) {
		printf("cuParamSeti (a) failed: res = %u\n", res);
		return -1;
	}
	res = cuParamSeti(function, 8, b_dev);
	if (res != CUDA_SUCCESS) {
		printf("cuParamSeti (b) failed: res = %u\n", res);
		return -1;
	}
	res = cuParamSeti(function, 12, b_dev >> 32);
	if (res != CUDA_SUCCESS) {
		printf("cuParamSeti (b) failed: res = %u\n", res);
		return -1;
	}
	res = cuParamSeti(function, 16, c_dev);
	if (res != CUDA_SUCCESS) {
		printf("cuParamSeti (c) failed: res = %u\n", res);
		return -1;
	}
	res = cuParamSeti(function, 20, c_dev >> 32);
	if (res != CUDA_SUCCESS) {
		printf("cuParamSeti (c) failed: res = %u\n", res);
		return -1;
	}
	res = cuParamSeti(function, 24, n);
	if (res != CUDA_SUCCESS) {
		printf("cuParamSeti (c) failed: res = %u\n", res);
		return -1;
	}
	res = cuParamSetSize(function, 28);
	if (res != CUDA_SUCCESS) {
		printf("cuParamSetSize failed: res = %u\n", res);
		return -1;
	}

	/* launch the kernel */
	res = cuLaunchGrid(function, n, 1);
	if (res != CUDA_SUCCESS) {
		printf("cuLaunchGrid failed: res = %u\n", res);
		return -1;
	}

	/* download c[] */
	res = cuMemcpyDtoH(c, c_dev, n*n * sizeof(unsigned int));
	if (res != CUDA_SUCCESS) {
		printf("cuMemcpyDtoH (c) failed: res = %u\n", res);
		return -1;
	}

	res = cuMemFree(a_dev);
	if (res != CUDA_SUCCESS) {
		printf("cuMemFree (a) failed: res = %u\n", res);
		return -1;
	}
	res = cuMemFree(b_dev);
	if (res != CUDA_SUCCESS) {
		printf("cuMemFree (b) failed: res = %u\n", res);
		return -1;
	}
	res = cuMemFree(c_dev);
	if (res != CUDA_SUCCESS) {
		printf("cuMemFree (c) failed: res = %u\n", res);
		return -1;
	}

	res = cuModuleUnload(module);
	if (res != CUDA_SUCCESS) {
		printf("cuModuleUnload failed: res = %u\n", res);
		return -1;
	}

	res = cuCtxDestroy(ctx);
	if (res != CUDA_SUCCESS) {
		printf("cuCtxDestroy failed: res = %u\n", res);
		return -1;
	}

	/* check the results */
	i = j = idx = 0;
	while (i < n) {
		while (j < n) {
			idx = i * n + j;
			if (c[idx] != a[idx] * b[idx]) {
				return -1;
			}
			j++;
		}
		i++;
	}

	free(a);
	free(b);
	free(c);

	return 0;
}