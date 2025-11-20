#include <stdio.h>
#include <math.h>
#include <mpi.h>
#include <stdlib.h>

#define HEAVY  100000
#define SIZE   30

// This function performs heavy computations, 
// its run time depends on x and y values
// DO NOT change the function, refer to it as a Black Box
double heavy(int x, int y) {
	int i, loop;
	double sum = 0;
	if ((x == 3 && y == 3) || (x == 3 && y == 5) ||
		(x == 3 && y == 7) || (x == 20 && y == 10))
		loop = 200;
	else
		loop = 1;
	for (i = 1; i < loop * HEAVY; i++)
		sum += cos(exp(cos((double)i / HEAVY)));
	return sum;
}

// -----------------------------------------------------
// Static Task Pool 
// -----------------------------------------------------
void static_operation(int numprocs, int myid, MPI_Status status){
    int x, y;
    int size = SIZE;
    int job_num = 0;
    double local_sum = 0;

    // Assign jobs based on job_num % numprocs
    for (x = 0; x < size; x++)
		for (y = 0; y < size; y++){
            if(job_num % numprocs == myid){
                local_sum += heavy(x,y);
            }
            job_num++;
        }

    // Process 0 collects results from all processes
    if(myid == 0){
        if(numprocs > 1){
            for(int i=1; i<numprocs; i++){
                double sum = 0;
                MPI_Recv(&sum, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, &status);
                local_sum += sum; 
            }
        }
        printf("static final answer = %e\n", local_sum);
    }
    else{
        // Send local result to process 0
        MPI_Send(&local_sum, 1, MPI_DOUBLE, 0, 10, MPI_COMM_WORLD);
    }
}

// ----------------------------------------------
// Helper function to get next point (x,y) in grid
// ----------------------------------------------
int* check_point(int x, int y){
    int* new_point = (int*)malloc(sizeof(int) * 2);
    // Move to next column or to next row if at end
    if(y < SIZE - 1){
        y++;
    }
    else{
        x++;
        y = 0;
    }
    new_point[0] = x;
    new_point[1] = y;
    return new_point;
}

// ----------------------------------------------
// Dynamic Task Pool 
// ----------------------------------------------
void dynamic_operation(int numprocs, int myid, MPI_Status status){
    int terminated_procs = 0;
    int number_of_jobs = SIZE * SIZE;

    // Master process logic
    if(myid == 0){
        int* point = (int*)malloc(sizeof(int) * 2);
        point[0] = 0;
        point[1] = 0;

        // Send initial jobs to all worker processes
        for(int i = 1; i < numprocs; i++){
            MPI_Send(point, 2, MPI_INT, i, 10, MPI_COMM_WORLD);
            number_of_jobs--;
            point = check_point(point[0], point[1]);
        }

        double sum = 0;
        double result = 0;

        // Continue until all workers terminate
        while(terminated_procs < numprocs - 1){
            // Receive result from any worker
            MPI_Recv(&sum, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, &status);
            result += sum;

            if(number_of_jobs == 0){
                // No more jobs so send termination signal (tag 20)
                MPI_Send(point, 2, MPI_INT, status.MPI_SOURCE, 20, MPI_COMM_WORLD);
                terminated_procs++;
            }
            else{
                // Send next job to the worker
                MPI_Send(point, 2, MPI_INT, status.MPI_SOURCE, 10, MPI_COMM_WORLD);
                number_of_jobs--;
                point = check_point(point[0], point[1]);
            }
        }

        printf("Dynamic final answer = %e\n", result);
    }
    // Worker process logic
    else{
        double sum = 0;
        while(1){
            int point[2];
            // Receive job from master (or termination signal)
            MPI_Recv(point, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            // Termination received so exit
            if(status.MPI_TAG == 20){
                break;
            }
            sum = heavy(point[0], point[1]);
            // Send result back to master
            MPI_Send(&sum, 1, MPI_DOUBLE, 0, 10, MPI_COMM_WORLD);
        }
    }
}

// ----------------------------------------------
// Sequential function
// ----------------------------------------------
void Sequential (){
    double answer = 0;
    for (int x = 0; x < SIZE; x++)
		for (int y = 0; y < SIZE; y++)
			answer += heavy(x, y);
    printf("answer = %e \n", answer);    
}

int main(int argc, char* argv[]) {

    int x, y;
    int size = SIZE;
    double answer = 0;
    double t1, t2;
    int myid, numprocs;
    MPI_Status status;

    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    // Start timer
    t1 = MPI_Wtime();
    // Sequential();
    static_operation(numprocs, myid, status);
    // dynamic_operation(numprocs, myid, status);
    // End timer
    t2 = MPI_Wtime();

    // Print run time (only by master)
    if (myid == 0){
        printf("Time : %e seconds \n", t2 - t1);
    }

    // Finalize MPI
    MPI_Finalize();
}
