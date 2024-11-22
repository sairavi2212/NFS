#include "headers.h"

#define DATA_SIZE 1024
#define SEND_TEXT



int chunk_send(int ss_sock, char *data, int data_len) {
    int offset = 0;             // Current offset in the data
    char field1[MBUFF];         // Packet to be sent

    while (offset < data_len) {
        // Fill the packet
        int chunk_size = (data_len - offset > sizeof(field1)) ? sizeof(field1) : (data_len - offset);
        memcpy(field1, data + offset, chunk_size);

        // Null-terminate if sending text data
        #ifdef SEND_TEXT
        if (chunk_size < sizeof(field1)) {
            field1[chunk_size] = '\0';
        }
        #endif
        // Send the packet
        if (nfs_send(ss_sock, PACKET, field1, NULL) == -1) {
            fprintf(stderr, "Data transmission failed at offset %d. The entire 'write' terminated. Please make the write request again\n");
            nfs_send(ss_sock, ERROR, "send failed" , NULL);
            return -1;
        }

        offset += chunk_size; // Move to the next chunk
    }

    // Send a STOP packet to indicate the end of transmission
    if (nfs_send(ss_sock, STOP, NULL, NULL) == -1) {
        perror("chunk_send: nfs_recv : Failed to send STOP packet");
        return -1;
    }

    return 0;
}

// Currently uses only a single child thread, but added for future scalability
void wait_for_ack2(void * int_arr)
{
    int* index = (int *)int_arr;
    int thread_index =  index[0];
    int ss_sock = index[1];
    int ns_sock = index[2];
    nfs_comm ss_response;
    if (nfs_recv(&ss_response , ss_sock) == -1)
    {
        fprintf(stderr, "Failed to receive ack from SS\n");
    } 
    if (ss_response.type == ACK2)
    {
        printf("Asynchronous Write operation successful\n");
    }
    else if (ss_response.type == ERROR)
    {
        printf("Error in asynchronous write operation: %s\n", ss_response.field1);
    }
    close(ss_sock);

    // Send COmpletion ack to NS to allow other writers, readers to access the file
    nfs_send(ns_sock, ACK1, NULL, NULL);
    close(ns_sock);
    pthread_mutex_lock(&thread_mutex);
    thread_busy[thread_index] = false;
    pthread_mutex_unlock(&thread_mutex);

}

// MODIFY THE READINGF FUNCTION


int wait_for_acks(int ss_sock, int ns_sock)
{   
    nfs_comm ss_response;
    if (nfs_recv(&ss_response, ss_sock) == -1)
    {
        fprintf(stderr, "Failed to receive ack from SS\n");
        return -1;
    }
    if (ss_response.type == ERROR)
    {   
        fprintf(stderr, "ERROR from SS for write : %s\n", ss_response.field1);
        return -1;
    }
    else if (ss_response.type == ACK1)
    {
        printf("Data sent to SS\n");
        printf("Waiting for SS to save them to disk\n");
        int thread_index = -1;

        pthread_mutex_lock(&thread_mutex);
        while(thread_index == -1)
        {
            for (int i=0;i<NUM_THREADS;i++)
            {
                if (thread_busy[i]==false)
                {
                    thread_busy[i] = true;
                    thread_index = i;
                    break;
                }
            }
        }
        // Pass the index, so that the thread can mark itself as free
        int index[3] = {thread_index, ss_sock, ns_sock};
        pthread_create(&threads[thread_index], NULL, wait_for_ack2, (void *)&index);
        pthread_mutex_unlock(&thread_mutex);
    }
    return 0;
}





int ss_write(int ss_sock,int ns_sock,  char* file_path, int type, char choice)
{
    nfs_comm ss_response;
    memset(&ss_response, 0, sizeof(ss_response));

    char ch[2];
    ch[0] = choice;
    ch[1] = '\0';
    if (request_receive(ss_sock, type, file_path, ch, &ss_response) < 0){
        return -1;
    }
    #ifdef DEBUG
    printf("[DEBUG] Received response from SS for write\n");
    #endif
    int rtype = response_type_check(ss_response);
    if (rtype == REQ_SUCCESS)
    {
        #ifdef DEBUG
        printf("[DEBUG] Write request accepted\n");
        #endif
        printf("Enter content to write:\n");
         int c;
        while ((c = getchar()) != '\n' && c != EOF) {
            // Clear all input until a newline is encountered
        }
        char data[DATA_SIZE];
        // Scan all data until newline
        fflush(stdout);
        fgets(data, DATA_SIZE, stdin);
        
         
        if (chunk_send(ss_sock, data , DATA_SIZE) == -1)
        {
            return -1;
        }
        // this fucntion closes ss_sock and ns_sock -- assume the function is not called whren ss_write returns -1
        wait_for_acks(ss_sock,ns_sock);
    }


    return 0;
}
        
        

    


int handle_write(char* file_path, int type, char choice, int ns_sock)
{
    #ifdef DEBUG
    printf("[DEBUG] Initiated write request with NS\n");
    #endif



    nfs_comm ns_response;
    memset(&ns_response, 0, sizeof(ns_response));
    
    if (request_receive(ns_sock, type , file_path, NULL, &ns_response) == -1)
    {
        close(ns_sock);
        return -1;
    }

    #ifdef DEBUG
    printf("[DEBUG] Received response from NS\n");
    #endif

    // Prints error message and returns the error code 
    int rtype = response_type_check(ns_response);
    if (rtype == REQ_SUCCESS)
    {
        // Connect to storage server
        #ifdef DEBUG
        printf("[DEBUG] Server info received- ip:%s port:%d\n", ns_response.field1, atoi(ns_response.field2));
        #endif
        int ss_sock = connect_to_server(ns_response.field1, atoi(ns_response.field2));
        if (ss_sock == -1) {
            printf("Error in connecting to storage server\n");
            return -1;
        }
        int rcode = ss_write(ss_sock,ns_sock, file_path, type, choice);
        if (rcode == -1)
        {
            close(ss_sock);
        }
        return rcode;
    }
    return rtype;
}