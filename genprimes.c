#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]){
	if(argc!=3){
		printf("ERROR: Input consists OF N and t\n");
		return -1;
	}

	/*Part 1: input*/
	unsigned int N=atoi(argv[1]),
			 	 t=atoi(argv[2]),
			 	 i, j, k;
	double 		 tstart=0,
				 ttaken;
				 //tloop=0;
	char* 		 filename=(char*)malloc(sizeof(argv[1])+sizeof(char)*5);
	int*  		 nums=(int*)malloc(sizeof(int)*(N+1));						//nums: [(0)_, (1)_, (2)2, (3)3, ... (N)N]
	if(N<=2 || N>100000){
		printf("ERROR: N should be between 3 and 100000\n");
		return -2;
	}
	else if(t<1 || t>100){
		printf("ERROR: t should be between 1 and 100\n");
		return -3;
	}

	/*Part 2: generate prime numbers*/
	tstart=omp_get_wtime();
	//step 1:generate all numbers from 2 to N (skipped)
	//step 2:starting from i=2, cross all multiples of i until N by marking the multiples -1
	//end when i=floor((N+1)/2) --> integer division
	#pragma omp parallel num_threads(t) default(none) private(i, j) shared(N, nums)
	for(i=2; i<(N+1)/2; i++)
		if(nums[i]==-1)
			continue;
		else{
			nums[i]=i;
			#pragma omp for
			/*
			for(j=i+i; j<=N; j+=i)
				nums[j]=-1;
			*/
			for(j=i+1; j<=N; j++)
				if(nums[j]==-1)
					continue;
				else if(j%i==0)
					nums[j]=-1;
				else
					nums[j]=j;
		}
	ttaken=omp_get_wtime()-tstart;
	printf("Time taken for the main part: %f\n", ttaken);
	/*
	#pragma omp parallel num_threads(t) default(none) private(i, j, ttaken) shared(N, nums, tloop, tstart)
	{
		#pragma omp single nowait
		{
			ttaken=omp_get_wtime()-tstart;
			printf("Time taken for the setup: %f\n", ttaken);
		}
		for(i=2; i<(N+1)/2; i++)
			if(nums[i]==-1)
				continue;
			else{
				nums[i]=i;
				ttaken=omp_get_wtime();
				#pragma omp for
				for(j=i+1; j<=N; j++)
					if(nums[j]==-1)
						continue;
					else if(j%i==0)
						nums[j]=-1;
					else
						nums[j]=j;
				#pragma omp single nowait 
				tloop+=(omp_get_wtime()-ttaken);
			}
	}
	ttaken=omp_get_wtime()-tstart;
	printf("Time taken for inner loop: %f\n", tloop);
	printf("Time taken for the main part: %f\n", ttaken);
	*/
	/*Part 3: output*/
	sprintf(filename, "%s%s", argv[1], ".txt");
	FILE *fp=fopen(filename, "w");
	if(fp==NULL){
		printf("ERROR: Failed to create output file\n");
		return -4;
	}
	else{
		j=1;
		fprintf(fp, "%d, %d, %d\n", j, nums[2], 0);
		k=nums[2];
		j++;
		for(i=3; i<=N; i++)
			if(nums[i]!=-1){
				fprintf(fp, "%d, %d, %d\n", j, nums[i], nums[i]-k);
				k=nums[i];
				j++;
			}
	}
	fclose(fp);

	free(filename);
	return 0;
}