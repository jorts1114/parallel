#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#define SUBMATRIX_SIZE 10000
#define BLOCK_SIZE 16

float getnum() {
  return rand()/((float) RAND_MAX);
}


__global__ void gpu_matrix_multiply(float *a, float *b, float *c, int n) 
{
  __shared__ float tile_a[BLOCK_SIZE][BLOCK_SIZE];
  __shared__ float tile_b[BLOCK_SIZE][BLOCK_SIZE];

  int row = blockIdx.y * BLOCK_SIZE + threadIdx.y;
  int col = blockIdx.x * BLOCK_SIZE + threadIdx.x;
  float tmp = 0.0f;
  int idx;

  for (int sub = 0; sub < gridDim.x; ++sub) // gridDim.x
    {
      idx = row * n + sub * BLOCK_SIZE + threadIdx.x;
      if(idx >= n*n)
        {
	  // n may not divisible by BLOCK_SIZE
	  tile_a[threadIdx.y][threadIdx.x] = 0.0f;
        }
      else
        {
	  tile_a[threadIdx.y][threadIdx.x] = a[idx];
        }

      idx = (sub * BLOCK_SIZE + threadIdx.y) * n + col;
      if(idx >= n*n)
        {
	  tile_b[threadIdx.y][threadIdx.x] = 0.0f;
        }  
      else
        {
	  tile_b[threadIdx.y][threadIdx.x] = b[idx];
        }
      __syncthreads();

      for (int k = 0; k < BLOCK_SIZE; ++k) 
        {
	  tmp += tile_a[threadIdx.y][k] * tile_b[k][threadIdx.x];
        }
      __syncthreads();
    }
  if(row < n && col < n)
    {
      c[row * n + col] = tmp;
    }
}


void make_identity(float* a, int startx, int starty, int length, int n) {
  // fill a with the identity from start to end
  int i, j;
  for (i=startx; i<length+startx; i++) {
    for (j=starty; j<length+starty; j++) {
      a[i*n+j] = (i-startx==j-starty) ? 1.0f : 0.0f;
    }
  }
}

void make_negidentity(float* a, int startx, int starty, int length, int n) {
  // fill a with the identity from start to end
  int i, j;
  for (i=startx; i<length+startx; i++) {
    for (j=starty; j<length+starty; j++) {
      a[i*n+j] = (i-startx==j-starty) ? -1.0f : 0.0f;
    }
  }
}

void make_x(float* x, int length) {
  int i, j;
  for (i=0; i<length; i++) {
    for (j=0; j<length; j++) {
      x[i*length+j] = getnum();
    }
  }
}

void make_zero(float* a, int startx, int starty, int length, int n) {
  int i, j;
  for (i=startx; i<length+startx; i++) {
    for (j=starty; j<length+starty; j++) {
      a[i*n+j] = 0.0f;
    }
  }
}

void copy_x(float* a, float* x, int startx, int starty, int length, int n) {
  int i, j;
  for (i=startx; i<length+startx; i++) {
    for (j=starty; j<length+starty; j++) {
      a[i*n+j] = x[(i-startx)*length+(j-starty)];
    }
  }
}

void copy_2x(float* a, float* x, int startx, int starty, int length, int n) {
  int i, j;
  for (i=startx; i<length+startx; i++) {
    for (j=starty; j<length+starty; j++) {
      a[i*n+j] = 2*x[(i-startx)*length+(j-starty)];
    }
  }
}

void copy_negx(float* a, float* x, int startx, int starty, int length, int n) {
  int i, j;
  for (i=startx; i<length+startx; i++) {
    for (j=starty; j<length+starty; j++) {
      a[i*n+j] = (-1)*x[(i-startx)*length+(j-starty)];
    }
  }
}

void make_result(float* a, int length) {
  int i, j;
  int half = length>>1;
  for (i=0; i<length; i++) {
    for (j=0; j<length; j++) {
      if (i == j) {
	if (i>=half)
	  a[i*length+j] = -1.0f;
	else
	  a[i*length+j] = 1.0f;
      }
      else
	a[i*length+j] = 0.0f;
    }
  }
}

float rothVerf(float* a, float* b, int n) {
  float sum = 0;
  int i, j;
  for (i=0; i<n; i++) {
    for (j=0; j<n; j++) {
      sum += (float) fabs(a[i*n+j] - b[i*n+j]);
    }
  }
  return sum;
}

void print_mat(float* a, int n) {
  int i, j;
  for (i=0; i<n; i++) {
    for (j=0; j<n; j++) {
      printf("%.2f\t", a[i*n+j]);
    }
    printf("\n");
  }
  printf("\n");
}


int main() {
  srand(100);
  int n = 2*SUBMATRIX_SIZE;
  int half = n>>1;
  size_t totalsize = sizeof(float)*n*n;
  size_t halfsize = sizeof(float)*half*half;
  float *x, *a, *b, *c, *d;

  cudaMallocHost((void**) &a, totalsize);
  cudaMallocHost((void**) &b, totalsize);
  cudaMallocHost((void**) &c, totalsize);
  cudaMallocHost((void**) &d, totalsize);
  cudaMallocHost((void**) &x, halfsize);
  
  if ((x==NULL) || (a==NULL) || (b==NULL) || (c==NULL) ||
      (d==NULL)) {
    printf("Matrix allocation error on host\n");
    exit(1);
  }

  make_x(x, half);

  // construct first matrix
  make_identity(a, 0, 0, half, n);
  copy_x(a, x, 0, half, half, n);
  make_zero(a, half, 0, half, n);
  make_identity(a, half, half, half, n);

  // second matrix
  make_identity(b, 0, 0, half, n);
  copy_2x(b, x, 0, half, half, n);
  make_zero(b, half, 0, half, n);
  make_negidentity(b, half, half, half, n);

  // third
  make_identity(c, 0, 0, half, n);
  copy_negx(c, x, 0, half, half, n);
  make_zero(c, half, 0, half, n);
  make_identity(c, half, half, half, n);

  // result
  make_result(d, n);

  // allocate on device
  float *dev_a, *dev_b, *dev_c, *dev_inter;
  cudaMalloc((void**) &dev_a, totalsize);
  cudaMalloc((void**) &dev_b, totalsize);
  cudaMalloc((void**) &dev_c, totalsize);
  cudaMalloc((void**) &dev_inter, totalsize);
  
  // copy to device
  cudaMemcpy(dev_a, a, totalsize, cudaMemcpyHostToDevice);
  cudaMemcpy(dev_b, b, totalsize, cudaMemcpyHostToDevice);
  cudaMemcpy(dev_c, c, totalsize, cudaMemcpyHostToDevice);

  unsigned int grid_rows = n / BLOCK_SIZE;
  unsigned int grid_cols = n / BLOCK_SIZE;
  dim3 dimGrid(grid_cols, grid_rows);
  dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE);

  // intermediate matrix product
  gpu_matrix_multiply<<<dimGrid, dimBlock>>>(dev_a, dev_b, dev_inter, n);
  cudaThreadSynchronize();

  // reuse old matrix
  gpu_matrix_multiply<<<dimGrid, dimBlock>>>(dev_inter, dev_c, dev_a, n);

  // bring product back to cpu
  cudaMemcpy(a, dev_a, totalsize, cudaMemcpyDeviceToHost);
  cudaThreadSynchronize();

  // check a against the result d
  float sum = rothVerf(a, d, n);
  printf("Total Error: %f\n", sum);

  // cleanup and exit
  cudaFree(dev_a);
  cudaFree(dev_b);
  cudaFree(dev_c);
  cudaFree(dev_inter);
  cudaFreeHost(a);
  cudaFreeHost(b);
  cudaFreeHost(c);
  cudaFreeHost(d);
  cudaFreeHost(x);
  return 0;
}
