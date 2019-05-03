#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <cuda.h>

__global__ void getmaxcu(unsigned int num[], unsigned int size, unsigned int gap)
{
  unsigned int i=gap,                                                      //loop variables
               start = (threadIdx.x)*gap;
  if(start%2!=0 || size<=0)
    return;
  else{
    //bottom-up tree search
    while(start+i<size && start%(2*i)==0){
      if(num[start]<num[start+i])
        num[start]=num[start+i];
      i*=2;
      __syncthreads();
    }
  }
}

int main(int argc, char *argv[])
{
    if(argc !=2)
    {
       printf("usage: maxgpu num\n");
       printf("num = size of the array\n");
       exit(1);
    }

    cudaError_t  err;                                                   // error var
    unsigned int size = 0, backup_size,                                 // The size of array
                 thread_num,
                 i;                                                     // loop index
    struct cudaDeviceProp prop;                                         // specs
    unsigned int *numbers, *cudanumbers,                                // pointers to array
                 *max=(unsigned int *)malloc(sizeof(unsigned int));     // pointers to max number
   
    size = atol(argv[1]);
    backup_size=size;
    numbers = (unsigned int *)malloc(size * sizeof(unsigned int));

    if(!numbers)
    {
       printf("Unable to allocate mem for an array of size %u\n", size);
       exit(1);
    }    
    srand(time(NULL)); // setting a seed for the random number generator
    // Fill-up the array with random numbers from 0 to size-1 
    for( i = 0; i < size; i++)
      numbers[i] = rand() % size;

    cudaGetDeviceProperties(&prop, 0);
    cudaMalloc((void**)&cudanumbers, size * sizeof(unsigned int));
    cudaMemcpy(cudanumbers, numbers, size * sizeof(unsigned int), cudaMemcpyHostToDevice);
    thread_num=ceil(prop.maxThreadsPerBlock/32)*32;
    unsigned int offset=0;
    i=0;
    //search 1024 elements at once; search remaining elements
    //structure: |----1024----| |----1024----| ... |size%1024| left to right
    while(offset<backup_size){
      if(backup_size-offset>=thread_num){
        getmaxcu<<<1, thread_num>>>((cudanumbers+offset), thread_num, 1);
      }
      else{
        getmaxcu<<<1, backup_size-offset>>>((cudanumbers+offset), backup_size-offset, 1);
      }
      getmaxcu<<<1, 1>>>((cudanumbers), backup_size, offset);
      offset+=thread_num;
    }
    cudaMemcpy(max, cudanumbers, sizeof(unsigned int), cudaMemcpyDeviceToHost);
    err=cudaGetLastError();
    if(err!=cudaSuccess){
      printf("CUDA error: %s\n", cudaGetErrorString(err));
      exit(-1);
    }
    cudaThreadSynchronize();
    cudaFree(cudanumbers);

    printf("CUDA returns: %d\n", *max);
    //printf("Sequential returns: %u\n", getmax(numbers, backup_size));

    free(numbers);
    free(max);

    exit(0);
}

/*
unsigned int getmax(unsigned int num[], unsigned int size)
{
  unsigned int i, j;
  unsigned int max = num[0];

  for(i = 1; i < size; i++)
    if(num[i] > max){
        max = num[i];
        j=i;
    }

  //printf("Sequential found max at %d\n", j);
  return max;
}

__global__ void printArray(unsigned int arr[], unsigned int length, unsigned int jump){
  for(int i = 0; i+jump<length; i++)
    printf("%d: %d\n", i+jump, arr[i+jump]);
}

void search(unsigned int arr[], unsigned int size, unsigned int target){
  printf("CUDA found max at");
  for(int i = 1; i < size; i++)
    if(arr[i] == target)
       printf(" %d ", i);
  printf("\n");
}
*/