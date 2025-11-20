#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>
#include <omp.h>
#include <math.h>
// This function initializes the matrix with random numbers (by process 0)
// Then it sends each row to process 1 using MPI_Send
void divide_matrix(int rank, int** A, int N, int M, MPI_Status status){
    if(rank == 0){
        // Fill the matrix with random numbers between 1 and 1000
        for(int i=0 ; i<N ; i++){
            for(int j=0 ; j<M ; j++){
                A[i][j] = rand() % 1000 + 1;
            }
        }
        // Send each row of the matrix to process 1
        for (int i = 0; i < N; ++i) {
            MPI_Send(A[i], M, MPI_INT, 1, 6+i, MPI_COMM_WORLD);
        }
    }
    else{
        // Receive each row from process 0
        for (int i = 0; i < N; ++i) {
            MPI_Recv(A[i], M, MPI_INT, 0, 6+i, MPI_COMM_WORLD, &status);
        }
    }
}
// This function calculates the sum of logs of all odd numbers in a KxK submatrix
double compute_log_sum(int** A, int K, int row, int column){
    int result = 0;
    for(int i=row ; i<row+K ; i++){
        for(int j=column ; j<column+K ; j++){
            if(A[i][j] % 2 == 1){
                result += log(A[i][j]);
            }
        }
    }
    return result;
}
// This function compares the best result between both processes and prints the overall best
void check_max_process_result(int rank, double best_sum, int* best_position, MPI_Status status){
    if(rank == 0){
        // Receive result from process 1
        double other_process_result;
        int* other_process_position = (int*)malloc(sizeof(int) * 2);
        MPI_Recv(&other_process_result, 1, MPI_DOUBLE, 1, 5, MPI_COMM_WORLD, &status);
        MPI_Recv(other_process_position, 2, MPI_INT, 1, 4, MPI_COMM_WORLD, &status);
        // Compare results and print the best position
        if(other_process_result > best_sum){
            printf("the sub matrix starts at row = %d and column = %d\n", other_process_position[0], other_process_position[1]);
        }
        else{
            printf("the sub matrix starts at row = %d and column = %d\n", best_position[0], best_position[1]);           
        }
    }
    else{
        // Send result to process 0
        MPI_Send(&best_sum, 1, MPI_DOUBLE, 0, 5, MPI_COMM_WORLD);
        MPI_Send(best_position, 2, MPI_INT, 0, 4, MPI_COMM_WORLD);
    }
}
// This function scans half the matrix in each process and finds the submatrix with the highest log sum
void find_best_K_sub(int rank, int** A, int N, int M, int K, MPI_Status status){
    int half_N = 0;
    if(rank == 1){
        half_N = N/2;// Process 1 handles bottom half
    }
    int* best_position = (int*)malloc(sizeof(int) * 2);
    best_position[0] = -1;
    best_position[1] = -1;
    double best_sum = -1;
    double result = 0;
    // Parallelize the search over rows using OpenMP
    #pragma omp parallel shared(A, N, M, K, best_position) private(result) num_threads(4)
    {
        // printf("Hello from thread %d out of %d in process %d\n", omp_get_thread_num(), omp_get_num_threads(), rank);
        #pragma omp for
        for(int i = half_N; i < N/2 + half_N; i++){
            if(i + K - 1 >= N) 
                continue;

            for(int j = 0; j < M; j++){
                if(j + K - 1 >= M) 
                    continue;

                double local_result = compute_log_sum(A, K, i, j);
                // Critical section to update the global best result safely
                #pragma omp critical
                {
                    if(local_result > best_sum){
                        best_sum = local_result;
                        best_position[0] = i;
                        best_position[1] = j;
                    }
                }
            }
        }
    }
    // Communicate the best submatrix result back to process 0
    check_max_process_result(rank, best_sum, best_position, status);
}


int main(int argc, char* argv[]) {
    int rank, numprocs;
    MPI_Status status;
    double start_time, end_time;
    start_time = omp_get_wtime();
    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    // Check if exactly 2 processes are running
    if(numprocs != 2){
        if (rank == 0)
            printf("you need to run two different processes\n");
        MPI_Finalize();
    }
    // Validate arguments: N M K
    if (argc < 4) {
        if (rank == 0)
            printf("you need to pass 4 arguments\n");
        MPI_Finalize();
    }

    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    int K = atoi(argv[3]);
    // Input validation for matrix and submatrix sizes
    if(K > N/2 || K < 50){
        if (rank == 0)
            printf("K is bigger than N/2 or less than 50\n");
        MPI_Finalize();
    }
    if(N < 50){
        if (rank == 0)
            printf("N is less than 50\n");
        MPI_Finalize();
    }
    if(M < 50){
        if (rank == 0)
            printf("M is less than 50\n");
        MPI_Finalize();
    }
    // Allocate 2D matrix dynamically
    int** A = (int**)malloc(sizeof(int*) * N);
    for(int i=0 ; i<N ; i++){
        A[i] = (int*)malloc(sizeof(int) * M);
    }
    // Distribute or receive the matrix
    divide_matrix(rank, A, N, M, status);
    // Each process finds the best KxK submatrix in its half
    find_best_K_sub(rank, A, N, M, K, status);

    end_time = omp_get_wtime();

    // Print run time (only by master)
    if (rank == 0){
        printf("Time : %e seconds \n", end_time - start_time);
    }
    // Finalize MPI
    MPI_Finalize();
}