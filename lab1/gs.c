#define _GNU_SOURCE

#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
<File Format>
Line 0: number_of_unknowns
Line 1: absolute_relative_error
Line 2: Initial_values_of_unknowns
Line 3 and more: coefficients_and_constant_of_each_equation

Given n unknowns, there are n equations.
*/

int main(int argc, const char* argv[]){
	if(argc!=2){
		printf("ERROR: Input 1 File.");
		return 1;
	}

	MPI_Init(NULL, NULL);
	//common, main varaibles
	unsigned int rank, 											//current process rank
				 offset,										//where current process starts in coefficients_and_constant
				 num_equation,									//work per process
				 num_unknown, 									//# of unknowns
				 len_unknown,									//# of digits of the # of unknowns
				 num_process,									//number of processes
				 padding=0,										//# of additional equations (num_known+1 0's) to num_equation%num_process=0.
				 repeat=1, index=0, x_rank, i, j, iteration=0;	//loop variables
	clock_t		 Tstart, Tend, Tcumulative1=0, Tcumulative2=0;	//timer variables
	double 		 absolute_relative_error,						//given error range
				 x=0;											//new unknown value
	double		 *performance_metrics=(double*)malloc(sizeof(double)*4), 
				 *coefficients_and_constant, 					//coefficients and constant(s) of >=1 row
		  		 *new_values,									//new values calculated during loop
		  		 *temp,
		  		 *ini_values; 									//initial values of unknowns with a gap of 1 block in between to store error
	MPI_Comm_size(MPI_COMM_WORLD, &num_process);	
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	memset(performance_metrics, 0, 4*sizeof(double));
	FILE *fp=fopen(argv[1], "r");
	if(fp==NULL){
		if(rank==0)
			printf("ERROR: Failed to open input file.");
		MPI_Abort(MPI_COMM_WORLD, MPI_SUCCESS);
		MPI_Finalize();
		return 2;
	}

	//task 1: read input from file and add padding if needed
	Tstart=clock();
	for(;;){
		unsigned int line_num=0,								//current line number
					 row_num=0;									//also accounts for which unknown
		char 		 delim[]=" ";								//token
		size_t 		 len;										//getline() variable			
		ssize_t 	 nread;										//# of characters read from line, including '\n'.
		char 		 *line=NULL; 								//current line 

		while((nread=getline(&line, &len, fp))!=-1){
			switch(line_num){
				//number of unknowns
				case 0: num_unknown=strtol(line, NULL, 10);
						if(num_unknown==0){
							if(rank==0)
								printf("WARNING: First line is 0.\n");
							MPI_Abort(MPI_COMM_WORLD, MPI_SUCCESS);
							MPI_Finalize();
							return 3;
						}
						len_unknown=nread-1; //# of characters read, except '\n'. It will be used when writing the output file
						padding=(num_unknown%num_process==0)?0:num_process-(num_unknown%num_process);
						num_equation=(num_unknown+padding)/num_process;
						coefficients_and_constant=(double*)malloc(sizeof(double)*(padding+num_unknown)*(2+num_unknown)); //each equation follows [x_rank, coefficients, constant]
						ini_values=(double*)malloc(sizeof(double)*(num_unknown+padding+1));
						new_values=(double*)malloc(sizeof(double)*(num_unknown+padding+1));
						offset=rank*num_equation*(num_unknown+2);
						break;
				//absolute relative error
				case 1: absolute_relative_error=strtof(line, NULL)*num_unknown;
						break;
				//initial values for each unknown
				case 2: ini_values[0]=0; //when MPI_ALLreduce() is used with MPI_SUM, the errors will be added.
						ini_values[1]=strtof(strtok(line, delim), NULL);
						for(i=2; i<=num_unknown; i++)
							ini_values[i]=strtof(strtok(NULL, delim), NULL);
						j=num_unknown+padding+1;
						while(i<j){
							ini_values[i]=0;
							i++;
						}
						break;
				//coefficients and constant for each equation. 
				default:coefficients_and_constant[index]=row_num;
						index++;
						coefficients_and_constant[index]=strtof(strtok(line, delim), NULL);
						for(i=1; i<=num_unknown; i++){
							index++;
							coefficients_and_constant[index]=strtof(strtok(NULL, delim), NULL);
						}
						index++;
						row_num++;
						break;
			}
			line_num++;
		}
		for(i=0; i<padding; i++){
			coefficients_and_constant[index]=row_num;
			index++;
			row_num++;
			for(j=1; j<(num_unknown+2); j++){
				coefficients_and_constant[index]=0;
				index++;
			}
		}
		free(line);
		break;
	}
	Tend=clock();
	performance_metrics[0]=(double)(Tend-Tstart)/CLOCKS_PER_SEC;
	
	//task 2: repeatedly compute unknown's values with the initial values until the error falls beneath the absolute_relative_error
	//		  -> each process, including p0, does the calcuation of its unknown and absolute relative error for its assigned equation. 
	//		  -> each process calls MPI_Allreduce() with MPI_SUM with the leading element being the sum of all errors.
	//		  -> if the sum of errors is larger than absolute_relative_error, that is, given error * num_unknown, the processes repeat without needing a separate call.
	while(repeat==1){
		iteration++;
		index=offset;
		//after adding, immediately set ini_values elements that are not assigned to this process to 0 so that MPI_Allreduce() will collect everything in an orderly fashion.
		memset(new_values, 0, (num_unknown+padding+1)*sizeof(double));
		Tstart=clock();
		for(i=0; i<num_equation; i++){
			x=0;
			x_rank=coefficients_and_constant[index];
			index++;
			for(j=0; j<x_rank; j++){
				x-=(coefficients_and_constant[index]*ini_values[j+1]);
				index++;
			}
			index++;
			for(j=x_rank+1; j<num_unknown; j++){
				x-=(coefficients_and_constant[index]*ini_values[j+1]);
				index++;
			}
			x+=coefficients_and_constant[index];
			j=(int)((x_rank+1)+(i*(num_unknown+2))+offset);
			if(coefficients_and_constant[j]>0)
				x/=coefficients_and_constant[j];
			else
				x=0;
			if(x!=0)
				new_values[0]+=fabs(((x-ini_values[x_rank+1])/x));
			new_values[x_rank+1]=x;
			index++;
		}
		Tcumulative1+=clock()-Tstart;

		if(num_process>1){
			Tstart=clock();
			MPI_Allreduce(MPI_IN_PLACE, new_values, num_unknown+padding+1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
			Tcumulative2=clock()-Tstart;
		}
		
		if(new_values[0]>absolute_relative_error)
			repeat=1;
		else
			repeat=0;
		temp=ini_values;
		ini_values=new_values;
		new_values=temp;
	}
	performance_metrics[1]=(double)Tcumulative1/CLOCKS_PER_SEC;
	performance_metrics[2]=(double)Tcumulative2/CLOCKS_PER_SEC;
	
	//task 3: write output to x.sol.txt
	Tstart=clock();
	if(rank==0){
		char* file_name=(char*)malloc(sizeof(char)*(len_unknown+5));
		sprintf(file_name, "%d.sol", (int)num_unknown);
		fp=fopen(file_name, "w");
		if(fp!=NULL){
			j=num_unknown+1;
			for(i=1; i<j; i++)
				fprintf(fp, "%f\n", ini_values[i]);
			fclose(fp);
		}
		else
			printf("ERROR: Cannot create %s.sol\n", file_name);
		printf("total number of iteration: %d\n", iteration);
		Tend=clock();
		if(num_process>1)
			MPI_Allreduce(MPI_IN_PLACE, performance_metrics, 4, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
		printf("input took %fs\n", performance_metrics[0]/num_process);
		printf("calculation took %fs\n", performance_metrics[1]/num_process);
		printf("communication took %fs\n", performance_metrics[2]/num_process);
		printf("output took %fs\n", performance_metrics[3]/num_process);
		printf("total time=%fs\n", (double)Tend/CLOCKS_PER_SEC);
	}
	else
		MPI_Allreduce(MPI_IN_PLACE, performance_metrics, 4, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

	//cleanup
	free(coefficients_and_constant);
	free(ini_values);
	free(new_values);
	free(performance_metrics);

	//conclusion
	MPI_Finalize();
	return 0;
}