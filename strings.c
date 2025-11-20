#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

// Reads input file and extracts K, N, MAX_ITERATIONS, the target substring, and all input strings
void read_file(int* K, int* N, int* MAX_ITERATIONS, char** substring, char*** strings) {
    FILE *fptr = fopen("data.txt", "r");
    if (!fptr) {
        printf("Failed to open file.\n");
        MPI_Abort(MPI_COMM_WORLD,1); 
    }
    // Read K, N, MAX_ITERATIONS
    if (fscanf(fptr, "%d %d %d", K, N, MAX_ITERATIONS) == EOF) {
        printf("Error reading K, N, MAX_ITERATIONS.\n");
        MPI_Abort(MPI_COMM_WORLD,1); 
    }

    // Read the substring we're searching for
    char* sub_string = (char*)malloc(sizeof(char) * (2 * (*N) + 1));
    if (fscanf(fptr, "%s", sub_string) == EOF) {
        printf("Error reading substring.\n");
        MPI_Abort(MPI_COMM_WORLD,1); 
    }
    sub_string[2 * (*N)] = '\0';
    *substring = sub_string;

    //Read the list of strings
    char** string_array = (char**)malloc(sizeof(char*) * (*K) * (*K));
    for (int i = 0; i < (*K) * (*K); i++) {
        string_array[i] = (char*)malloc(sizeof(char) * (2 * (*N) + 1));
        if (fscanf(fptr, "%s", string_array[i]) == EOF) {
            printf("you have less strings than %d\n", (*K) * (*K));
            MPI_Abort(MPI_COMM_WORLD,1); 
        }
        string_array[i][2 * (*N)] = '\0';
    }

    *strings = string_array;
    fclose(fptr);
}

// Verifies that all strings have the same length
void check_strings_length(char** strings, int K) {
    for (int i = 0; i < (K * K) - 1; i++) {
        if (strlen(strings[i]) != strlen(strings[i + 1])) {
            printf("The strings do not have the same length.\n");
            MPI_Abort(MPI_COMM_WORLD,1);            
        }
    }
}

// Distributes strings: rank 0 sends one string to each process
void divide_strings(char** strings, int size, int rank, char* process_string, int string_length, MPI_Status* status) {
    if (rank == 0) {
        strcpy(process_string, strings[0]);
        for (int i = 1; i < size; i++) {
            MPI_Send(strings[i], string_length, MPI_CHAR, i, 10, MPI_COMM_WORLD);
        }
    } else {
        MPI_Recv(process_string, string_length, MPI_CHAR, 0, 10, MPI_COMM_WORLD, status);
    }
}

// Extracts even or odd-indexed characters from a string
char* get_chars(char* process_string, int even_odd){
    char* chars = (char*)malloc(sizeof(char) * strlen(process_string));
    int counter = 0;
    for(int i=0;i<strlen(process_string);i++){
        if(i % 2 == even_odd){
            chars[counter] = process_string[i];
            counter++;
        }
    }
    chars[counter] = '\0';
    return chars;
}

// Updates the string using received characters (odd from left, even from down)
void update(char* process_string, char* odd_chars, char* even_chars){
    int counter = 0;
    for(int i=0;i<strlen(odd_chars);i++){
        process_string[counter] = odd_chars[i];
        counter++;
    }
    for(int i=0;i<strlen(even_chars);i++){
        process_string[counter] = even_chars[i];
        counter++;
    }
    process_string[counter] = '\0';
}

// Checks if the target substring exists in the current string
int check_substring(char* substring, char* process_string){
    if(strstr(process_string, substring) != NULL){
        return 1;
    }
    return 0;
}

// Checks if we found the substring in a process and send a flag that says if we need to finish the job of the processes or not
void check_substring_results(int rank, int K, int found_sub, MPI_Comm comm, int N, char* process_string, MPI_Status* status){
    // Gather results at rank 0
    if(rank == 0){
        int* found_sub_answrs = (int*)malloc(sizeof(int) * K * K);
        MPI_Gather(&found_sub, 1, MPI_INT, found_sub_answrs, 1, MPI_INT, 0, comm);
        int flag = 0;
        for(int j=0;j<K*K;j++){
            if(found_sub_answrs[j] == 1){
                flag = 1;
                break;
            }
        }
        // Tell everyone if should stop
        for(int p=1;p<K*K;p++){
            MPI_Send(&flag, 1, MPI_INT, p, 100, comm);
        }
        if(flag == 1){
            char* final_string = (char*)malloc(sizeof(char) * (2*N+1));
            printf("rank=%d, and the string is %s\n", rank, process_string);
            for(int z=1;z<K*K;z++){
                MPI_Recv(final_string, 2*N+1, MPI_CHAR, z, 50, comm, status);
                printf("rank=%d, and the string is %s\n", z, final_string);
            }
            MPI_Abort(comm, 1);
        }
    }
    else{
        MPI_Gather(&found_sub, 1, MPI_INT, NULL, 0, MPI_INT, 0, comm);
        int flag = 0;
        MPI_Recv(&flag, 1, MPI_INT, 0, 100, comm, status);
        if(flag==1){
            MPI_Send(process_string, 2*N+1, MPI_CHAR, 0, 50, comm);
        }
    }
}

// Each process sends/receives parts of its string and checks for the substring
void do_job(int MAX_ITERATIONS, MPI_Comm comm, char* process_string, MPI_Status* status, char* substring, int rank, int K, int N){
    int row_source, row_dest, column_source, column_dest;
    char* odd_chars, *even_chars;
    char* received_odd_chars, *received_even_chars;   
    for(int i=0;i<MAX_ITERATIONS;i++){
         // Set up neighbor communication
        MPI_Cart_shift(comm, 0, 1, &row_source, &row_dest);
        MPI_Cart_shift(comm, 1, 1, &column_source, &column_dest);
        // Split string
        odd_chars = get_chars(process_string, 1);
        even_chars = get_chars(process_string, 0);
        int odd_size = strlen(odd_chars);
        int even_size = strlen(even_chars);
         // Send to neighbors
        MPI_Send(odd_chars, odd_size+1, MPI_CHAR, row_source, 20, comm);
        MPI_Send(even_chars, even_size+1, MPI_CHAR, column_dest, 30, comm);
        // Receive from neighbors
        received_odd_chars = (char*)malloc(sizeof(char) * (odd_size + 1));
        received_even_chars = (char*)malloc(sizeof(char) * (even_size + 1));
        MPI_Recv(received_odd_chars, odd_size+1, MPI_CHAR, row_dest, 20, comm, status);
        MPI_Recv(received_even_chars, even_size+1, MPI_CHAR, column_source, 30, comm, status);
        // Reconstruct string
        update(process_string, received_odd_chars, received_even_chars);
         // Check if we found the target substring
        int found_sub = check_substring(substring, process_string);
        check_substring_results(rank, K, found_sub, comm, N, process_string, status);
        // Sync processes before next iteration
        MPI_Barrier(comm);
    }
    if(rank == 0){
        printf("The string was not found");
    }
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Comm comm;
    int K, N, MAX_ITERATIONS;
    char* substring = NULL;
    char** strings = NULL;
    MPI_Status status;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // Rank 0 loads the file and check the strings lengths
    if (rank == 0) {
        read_file(&K, &N, &MAX_ITERATIONS, &substring, &strings);
        check_strings_length(strings, K);
    }
    //Sends the K,N,Max iteration to everyone
    MPI_Bcast(&K, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&MAX_ITERATIONS, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int string_length = 2 * N + 1;
    if (rank != 0)
        substring = (char*)malloc(sizeof(char) * string_length);
    MPI_Bcast(substring, string_length, MPI_CHAR, 0, MPI_COMM_WORLD);
    if (size != K * K) {
        printf("Please run with %d processes.\n", K * K);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    // Create 2D Cartesian topology
    int dims[2] = {K, K};
    int periods[2] = {1, 1};
    int reorder = 1;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, reorder, &comm);
    // Distribute strings and begin processing
    char* process_string = (char*)malloc(sizeof(char) * string_length);
    divide_strings(strings, size, rank, process_string, string_length, &status);
    do_job(MAX_ITERATIONS, comm, process_string, &status, substring, rank, K, N);
    MPI_Finalize();
    return 0;
}