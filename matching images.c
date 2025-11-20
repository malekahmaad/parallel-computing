#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <omp.h>
#include <mpi.h>
#include <math.h>

// struct for the images and the objects
typedef struct {
    int id;
    int dimension;
    int** data;
} image;

// function to read the input file
void read_file(image** images, image** objects, double* matching_value, int* images_count, int* objects_count){
    FILE *fptr = fopen("Input.txt", "r");
    if(fptr == NULL){
        perror("no such file or directory\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    char buffer[100];
    fgets(buffer, 100, fptr);
    if(buffer[strlen(buffer) - 1] == '\n'){
        buffer[strlen(buffer) - 1] = '\0';
    }
    // reading the matching value
    char *endptr;
    *matching_value = strtod(buffer, &endptr);
    fgets(buffer, 100, fptr);
    if(buffer[strlen(buffer) - 1] == '\n'){
        buffer[strlen(buffer) - 1] = '\0';
    }

    //reading images
    *images_count = atoi(buffer);
    *images = (image*) malloc(sizeof(image) * (*images_count));
    for(int i=0 ; i<*images_count ; i++){
        fgets(buffer, 100, fptr);
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        (*images)[i].id = atoi(buffer);
        fgets(buffer, 100, fptr);
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        (*images)[i].dimension = atoi(buffer);
        (*images)[i].data = (int**)malloc(sizeof(int*) * (*images)[i].dimension);
        for(int j=0;j<(*images)[i].dimension;j++){
            ((*images)[i].data)[j] = (int*)malloc(sizeof(int)* (*images)[i].dimension);
        }
        for(int j=0;j<(*images)[i].dimension;j++){
            for(int k=0;k<(*images)[i].dimension;k++){
                fgets(buffer, 100, fptr);
                if(buffer[strlen(buffer) - 1] == '\n'){
                    buffer[strlen(buffer) - 1] = '\0';
                }
                ((*images)[i].data)[j][k] = atoi(buffer);
            }
        }
    }

    // reading objects
    fgets(buffer, 100, fptr);
    if(buffer[strlen(buffer) - 1] == '\n'){
        buffer[strlen(buffer) - 1] = '\0';
    }
    *objects_count = atoi(buffer);
    *objects = (image*) malloc(sizeof(image) * (*objects_count));
    for(int i=0 ; i<*objects_count ; i++){
        fgets(buffer, 100, fptr);
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        (*objects)[i].id = atoi(buffer);
        fgets(buffer, 100, fptr);
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        (*objects)[i].dimension = atoi(buffer);
        (*objects)[i].data = (int**)malloc(sizeof(int*) * (*objects)[i].dimension);
        for(int j=0;j<(*objects)[i].dimension;j++){
            ((*objects)[i].data)[j] = (int*)malloc(sizeof(int)* (*objects)[i].dimension);
        }
        for(int j=0;j<(*objects)[i].dimension;j++){
            for(int k=0;k<(*objects)[i].dimension;k++){
                fgets(buffer, 100, fptr);
                if(buffer[strlen(buffer) - 1] == '\n'){
                    buffer[strlen(buffer) - 1] = '\0';
                }
                ((*objects)[i].data)[j][k] = atoi(buffer);
            }
        }
    }
    fclose(fptr);
}

// function for the processes job (dynamic dividing job)
void dynamic_operation(int numprocs, int myid, MPI_Status status){
    int terminated_procs = 0;
    int count = 0;

    // master process logic
    if(myid == 0){
        
        // read input file
        image* images;
        image* objects;
        double matching_value;
        int images_count;
        int objects_count;
        read_file(&images, &objects, &matching_value, &images_count, &objects_count);
        
        int number_of_jobs = images_count;
        int x = 0;
        FILE *out_ptr = fopen("Output.txt", "a");
        
        // sending the matchin value and the objects to all the processes
        for(int i = 1; i < numprocs; i++){
            MPI_Send(&matching_value, 1, MPI_DOUBLE, i, 10, MPI_COMM_WORLD);
        }
        for(int i = 1; i < numprocs; i++){
            MPI_Send(&objects_count, 1, MPI_INT, i, 10, MPI_COMM_WORLD);
            for(int j=0;j<objects_count;j++){
                MPI_Send(&(objects[j].id), 1, MPI_INT, i, 10, MPI_COMM_WORLD);
                MPI_Send(&(objects[j].dimension), 1, MPI_INT, i, 10, MPI_COMM_WORLD);

                // flattening the matrix of integers to an array of integers to send it
                int *flat_data = (int*)malloc(sizeof(int) * objects[j].dimension * objects[j].dimension);
                for (int k = 0; k < objects[j].dimension; k++) {
                    for (int m = 0; m < objects[j].dimension; m++) {
                        flat_data[k * objects[j].dimension + m] = (objects[j].data)[k][m];
                    }
                }
                MPI_Send(flat_data, objects[j].dimension*objects[j].dimension, MPI_INT,
                     i, 10, MPI_COMM_WORLD);
                free(flat_data);
            }
        }

        // send initial jobs to all worker processes
        for(int i = 1; i < numprocs; i++){
            if(count >= images_count){
                MPI_Send(&x, 1, MPI_INT, i, 20, MPI_COMM_WORLD);
                terminated_procs++;
            }
            else{
                MPI_Send(&x, 1, MPI_INT, i, 10, MPI_COMM_WORLD);
                MPI_Send(&(images[count].id), 1, MPI_INT, i, 10, MPI_COMM_WORLD);
                MPI_Send(&(images[count].dimension), 1, MPI_INT, i, 10, MPI_COMM_WORLD);
                
                // flattening the matrix of integers to an array of integers to send it
                int *flat_data = (int*)malloc(sizeof(int) * images[count].dimension * images[count].dimension);
                for (int k = 0; k < images[count].dimension; k++) {
                    for (int j = 0; j < images[count].dimension; j++) {
                        flat_data[k * images[count].dimension + j] = (images[count].data)[k][j];
                    }
                }
                MPI_Send(flat_data, images[count].dimension*images[count].dimension, MPI_INT,
                     i, 10, MPI_COMM_WORLD);
                free(flat_data);
                count += 1;
                number_of_jobs --;
            }
        }

        // continue until all workers terminate
        while(terminated_procs < numprocs - 1){
            // receive result from any worker and save it in the output file
            char s[1024];
            MPI_Recv(s, 1024, MPI_CHAR, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, &status);
            fprintf(out_ptr, s);
            
            // check if there are no jobs to do and send termination tag (20)
            if(number_of_jobs == 0){
                MPI_Send(&x, 1, MPI_INT, status.MPI_SOURCE, 20, MPI_COMM_WORLD);
                terminated_procs++;
            }
            else{
                // send next job to the worker
                MPI_Send(&x, 1, MPI_INT, status.MPI_SOURCE, 10, MPI_COMM_WORLD);
                MPI_Send(&(images[count].id), 1, MPI_INT, status.MPI_SOURCE, 10, MPI_COMM_WORLD);
                MPI_Send(&(images[count].dimension), 1, MPI_INT, status.MPI_SOURCE, 10, MPI_COMM_WORLD);
                int *flat_data = (int*)malloc(sizeof(int) * images[count].dimension * images[count].dimension);
                for (int k = 0; k < images[count].dimension; k++) {
                    for (int j = 0; j < images[count].dimension; j++) {
                        flat_data[k * images[count].dimension + j] = (images[count].data)[k][j];
                    }
                }
                MPI_Send(flat_data, images[count].dimension*images[count].dimension, MPI_INT,
                     status.MPI_SOURCE, 10, MPI_COMM_WORLD);
                
                count += 1;
                number_of_jobs--;
            }
        }

        // free images
        for (int i = 0; i < images_count; i++) {
            for (int j = 0; j < images[i].dimension; j++) {
                free(images[i].data[j]);
            }
            free(images[i].data);
        }
        free(images);

        // free objects
        for (int i = 0; i < objects_count; i++) {
            for (int j = 0; j < objects[i].dimension; j++) {
                free(objects[i].data[j]);
            }
            free(objects[i].data);
        }
        free(objects);
        fclose(out_ptr);
    }
    // worker process logic
    else{
        // save the matching value and the objects
        double matching_value;
        MPI_Recv(&matching_value, 1, MPI_DOUBLE, 0, 10, MPI_COMM_WORLD, &status);
        int objects_count;
        MPI_Recv(&objects_count, 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
        image* objects = (image*) malloc(sizeof(image) * objects_count);
        for (int j = 0; j < objects_count; j++) {
            MPI_Recv(&(objects[j].id), 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
            MPI_Recv(&(objects[j].dimension), 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
            int dim = objects[j].dimension;
            objects[j].data = (int**)malloc(sizeof(int*) * dim);
            for (int k = 0; k < dim; k++) {
                objects[j].data[k] = (int*)malloc(sizeof(int) * dim);
            }
            // converting the array of integers back to a matrix of integers
            int *flat_data = (int*)malloc(sizeof(int) * dim * dim);
            MPI_Recv(flat_data, dim * dim, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
            for (int k = 0; k < dim; k++) {
                for (int m = 0; m < dim; m++) {
                    objects[j].data[k][m] = flat_data[k * dim + m];
                }
            }
            free(flat_data);
        }
        while(1){
            // receive job from master (or termination signal)
            int x;
            MPI_Recv(&x, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            
            // termination received so exit
            if(status.MPI_TAG == 20){
                break;
            }

            // receive a job form the master (image)
            image im;
            MPI_Recv(&(im.id), 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
            MPI_Recv(&(im.dimension), 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
            im.data = (int**) malloc(sizeof(int*) * im.dimension);
            int *flat_data = (int*)malloc(sizeof(int) * im.dimension * im.dimension);
            MPI_Recv(flat_data, im.dimension*im.dimension, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
            for (int i = 0; i < im.dimension; i++) {
                im.data[i] = &flat_data[i * im.dimension];
            }
            int match_row = -1;
            int match_col = -1;
            int found_match = 0;
            int object_id = -1;

            // make 4 omp threads 
            //the collapse(2) merges the two loops (objects × positions) into one iteration space
            //parallelize the nested loops over objects and all possible image positions using these 4 threads
            // the jobs are divided dynamically
            #pragma omp parallel for collapse(2) schedule(dynamic) num_threads(4)
            for (int i = 0; i < objects_count; i++) {
                for (int j = 0; j < im.dimension*im.dimension; j++) {
                    
                    // flag so we know we found a matching
                    if (found_match == 1) 
                        continue;

                    // calculate the matching value of the current position
                    int cur_row = j/im.dimension;
                    int cur_col = j%im.dimension;
                    if(cur_row + objects[i].dimension <= im.dimension && cur_col + objects[i].dimension <= im.dimension){
                        double value = 0.0;
                        for(int k=cur_row;k<cur_row + objects[i].dimension;k++){
                            for(int z=cur_col;z<cur_col + objects[i].dimension;z++){
                                value += fabs((im.data[k][z] - objects[i].data[k-cur_row][z-cur_col]) / (double)im.data[k][z]);
                            }
                        }
                        // see if its less than the matching value the we need to save this and exit
                        if(value < matching_value){
                            // enter this critical part and if no thread is inside it then check if some thread found a matching or not while this thread is waiting
                            #pragma omp critical
                            if(found_match == 0){
                                found_match = 1;
                                match_row = cur_row;
                                match_col = cur_col;
                                object_id = objects[i].id;
                            }    
                        }
                    }
                }
            }

            // create the result and send it to the master
            char s[1024] = "Picture ";
            if(found_match == 0){
                char temp[20];
                sprintf(temp, "%d", im.id);
                strcat(s, temp);
                strcat(s, " No Objects were found\n");
            }
            else{
                char temp[20];
                sprintf(temp, "%d", im.id);
                strcat(s, temp);
                strcat(s, " found Object ");
                sprintf(temp, "%d", object_id);
                strcat(s, temp);
                strcat(s, " in Position(");
                sprintf(temp, "%d", match_row);
                strcat(s, temp);
                strcat(s, ",");
                sprintf(temp, "%d", match_col);
                strcat(s, temp);
                strcat(s, ")\n");
            }
            MPI_Send(s, 1024, MPI_CHAR, 0, 10, MPI_COMM_WORLD);
            free(flat_data);
            free(im.data);
        }

        // free the objects after finishing all the jobs 
        for (int j = 0; j < objects_count; j++) {
            for (int k = 0; k < objects[j].dimension; k++) {
                free(objects[j].data[k]);
            }
            free(objects[j].data);
        }
        free(objects);
    }
}

int main(int argc, char* argv[]){
    int myid, numprocs;
    MPI_Status status;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    if(numprocs == 1){
        perror("you are running the program with one process\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    dynamic_operation(numprocs, myid, status);

    MPI_Finalize();
    return 0;
}
