#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

backup_node backup[SBUFF];
bool first_time_done;

int init_server_socket(int port)
{
    int server_fd;
    struct sockaddr_in address;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&address, '0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 100) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
#ifdef DEBUG
    printf("[DEBUG] Server listening on port %d...\n", port);
#endif
    return server_fd;
}

void delete_leaves_recursive(node *current, node *par, int index_in_parent, int storage_id)
{
    if (!current)
        return;

    bool is_leaf = true;

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (current->next_node[i])
        {
            is_leaf = false;
            delete_leaves_recursive(current->next_node[i], current, i, storage_id);
        }
    }

    if (is_leaf && current->belong_to_storage_server_id == storage_id)
    {

        if (par)
        {
            par->next_node[index_in_parent] = NULL;
        }
    }
}

void delete_all_leaves(int storage_id)
{
    node *tmp = parent; // Assuming `parent` is globally defined
    if (!tmp)
        return;

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (tmp->next_node[i])
        {
            delete_leaves_recursive(tmp->next_node[i], tmp, i, storage_id);
        }
    }
}

void *start_pinging(void *args)
{
    send_to_ping *data = (send_to_ping *)args;
    int sock = data->ping_socket;
    int storage_id = data->storage_id;
    struct timeval timeout;
    #ifdef DEBUG
    printf("[DEBUG] Starting pinging thread with %d\n", storage_id);
    #endif
    timeout.tv_sec = TIME_OUT_TIME; // Set timeout to 5 seconds
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        perror("Error setting socket timeout");
        return NULL;
    }
    while (1)
    {
        ping p;
        int stat = recv(sock, &p, sizeof(ping), MSG_WAITALL);
        if (stat <= 0)
        {
            pthread_mutex_lock(&ss_packet_copy_lock);
            storage_servers[storage_id].available = NOT_AVAILABLE;
            pthread_mutex_unlock(&ss_packet_copy_lock);
#ifdef DEBUG
            printf("[DEBUG] Storage server %d is not available\n", storage_id);
            char log_buff[MBUFF];
            snprintf(log_buff, MBUFF, "Storage server %d is not available", storage_id);
            log_into(log_buff);
            close(sock);
#endif
            break;
        }
#ifdef DEBUG
        // printf("[DEBUG] Received ping from storage server %d\n", sock);
#endif
    }
    return NULL;
}

int get_total_available()
{
    int cnt = 0;
    pthread_mutex_lock(&storage_cnt_lock);
    for (int i = 0; i < storage_count; i++)
    {
        pthread_mutex_lock(&ss_packet_copy_lock);
        if (storage_servers[i].available == AVAILABLE)
        {
            cnt++;
        }
        pthread_mutex_unlock(&ss_packet_copy_lock);
    }
    pthread_mutex_unlock(&storage_cnt_lock);
    return cnt;
}

backup_node get_backup_ss(int ssid)
{
    srand((unsigned int)time(NULL));
    backup_node b;
    b.bss1 = -1;
    b.bss2 = -1;
    if (storage_count <= 2)
    {
        return b;
    }
    int num = rand() % storage_count;
    int iter = num;
    int iter1 = -1;
    int iter2 = -1;
    for (int i = 0; i < 4 * storage_count; i++)
    {
        if (storage_servers[iter].available == AVAILABLE && iter != ssid)
        {
            if (iter1 == -1)
            {
                iter1 = iter;
            }
            else if (iter2 == -1)
            {
                if (iter2 != iter1)
                    iter2 = iter;
            }
            else
                break;
        }
        iter = (iter + 1) % storage_count;
    }
    b.bss1 = iter1;
    b.bss2 = iter2;
    return b;
}

int send_backup_data_modified (int type,char* path1,char* path2, int ssid,int bssid) {
    if (bssid == -1) return -1;
    nfs_comm backup_req;
    backup_req.type = type;
    snprintf(backup_req.field1, MBUFF, "%s:%d", storage_servers[bssid].ip, storage_servers[bssid].cl_port);
    snprintf(backup_req.field2, MBUFF, "%s:%s", path1, path2);
    int stat = send(socket_fd[ssid], &backup_req, sizeof(nfs_comm), 0);
    if (stat <= 0) {
        #ifdef DEBUG
        printf("[DEBUG] sending backup %d of %d original failed", bssid, ssid);
        #endif
        return -1;
    }
    #ifdef DEBUG
    printf("[DEBUG] Sending backup %d of %d original\n\tfield1 : %s\n\tfield2 : %s\n\ttype : %d\n", bssid, ssid, backup_req.field1, backup_req.field2, backup_req.type);
    #endif
    return 1;
}

int send_backup_data(int type,char* path1,char* path2,int bssid, int ssid) {
    if (bssid == -1) return -1;
    nfs_comm backup_req;
    backup_req.type = type;
    snprintf(backup_req.field1, MBUFF, "%s:%d", storage_servers[ssid].ip, storage_servers[ssid].cl_port);
    snprintf(backup_req.field2, MBUFF, "%s:%s", path1, path2);
    int stat = send(socket_fd[bssid], &backup_req, sizeof(nfs_comm), 0);
    if (stat <= 0)
    {
#ifdef DEBUG
        printf("[DEBUG] sending backup %d of %d original failed", bssid, ssid);
#endif
        return -1;
    }
#ifdef DEBUG
    printf("[DEBUG] Sending backup %d of %d original\n\tfield1 : %s\n\tfield2 : %s\n\ttype : %d\n", bssid, ssid, backup_req.field1, backup_req.field2, backup_req.type);
#endif
    return 1;
}

void send_to_leaves_recursive(node *current, node *par, int index_in_parent, int storage_id, int bssid)
{
    if (!current)
        return;

    bool is_leaf = true;

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (current->next_node[i])
        {
            is_leaf = false;
            send_to_leaves_recursive(current->next_node[i], current, i, storage_id, bssid);
        }
    }

    if (is_leaf && current->belong_to_storage_server_id == storage_id)
    {
        char path1[MBUFF];
        char path2[MBUFF];
        strcpy(path1, current->total_path);
        snprintf(path2, 2*MBUFF, "BACKUP/%s", path1);
        send_backup_data(COPY_REQ, path1 , path2, bssid, storage_id);
    }
}

void send_to_leaves_recursive_modified(node *current, node *par, int index_in_parent, int storage_id,int bssid)
{
    if (!current)
        return;

    bool is_leaf = true;

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (current->next_node[i])
        {
            is_leaf = false;
            send_to_leaves_recursive_modified(current->next_node[i], current, i, storage_id, bssid);
        }
    }

    if (is_leaf && current->belong_to_storage_server_id == storage_id)
    {
        char path1[MBUFF];
        char path2[MBUFF];
        strcpy(path2, current->total_path);
        snprintf(path1, 2*MBUFF, "BACKUP/%s", path2);
        #ifdef DEBUG
        printf("[DEBUG] Sending backup %s,%s in modified func\n", path1, path2);
        #endif
        send_backup_data_modified(BACKUP_REQ, path1 , path2, storage_id, bssid);
    }

}


void send_to_leaves_modified(int storage_id,int bssid)
{
    node *tmp = parent; // Assuming `parent` is globally defined
    if (!tmp)
        return;

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (tmp->next_node[i])
        {
            send_to_leaves_recursive_modified(tmp->next_node[i], tmp, i, storage_id, bssid);
        }
    }
}




void send_to_leaves(int storage_id, int bssid)
{
    node *tmp = parent; // Assuming `parent` is globally defined
    if (!tmp)
        return;

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (tmp->next_node[i])
        {
            send_to_leaves_recursive(tmp->next_node[i], tmp, i, storage_id, bssid);
        }
    }
}

int check_in_bef(char* ip,int port) {
    pthread_mutex_lock(&storage_cnt_lock);
    pthread_mutex_lock(&ss_packet_copy_lock);
    for (int i = 0; i < storage_count; i++)
    {
        if (strcmp(storage_servers[i].ip, ip) == 0 && storage_servers[i].ns_port == port)
        {
            storage_servers[i].available = AVAILABLE;
            pthread_mutex_unlock(&storage_cnt_lock);
            pthread_mutex_unlock(&ss_packet_copy_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&ss_packet_copy_lock);
    pthread_mutex_unlock(&storage_cnt_lock);
    return -1;
}

void get_ss_data() {
    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);
    int client_socket = accept(init_socket_ss, (struct sockaddr *)&client_address, &client_address_len);
    if (client_socket < 0)
    {
        log_into("Error in accepting connection from client");
        perror("Error in accepting connection from client");
        return;
    }
    initstorage *packet = malloc(sizeof(initstorage));
    if (!packet)
    {
        perror("Memory allocation failed for packet");
        close(client_socket);
        return;
    }
    int stat;
    while ((stat = recv(client_socket, packet, sizeof(*packet), MSG_WAITALL)) < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            continue;
        }
        else
        {
            perror("Error in receiving data from storage server");
            free(packet);
            close(client_socket);
            return;
        }
    }
    struct sockaddr_in ss_addr;
    int ss_fd;
    if ((ss_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket creation failed");
        return;
    }
    memset(&ss_addr, '0', sizeof(ss_addr));
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(packet->ns_port);
    if (inet_pton(AF_INET, packet->ip, &ss_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        printf("%s received\n", packet->ip);
        close(ss_fd);
        return;
    }
    char buff[SBUFF];
    int cond = 0;
    for (int i = 0; i < 8; i++)
    {
        if (connect(ss_fd, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) >= 0)
        {
            cond = 1;
            break;
        }
#ifdef DEBUG
        printf("[DEBUG] Connection failed, retrying...\n");
#endif
        sleep(1);
    }
    if (cond == 0)
    {
        perror("Connection failed");
        log_into("Connection failed");
        close(ss_fd);
        return;
    }
#ifdef DEBUG
    printf("[DEBUG] Connected to storage server %s:%d\n", packet->ip, packet->ns_port);
#endif
    char log_buff[MBUFF];
    snprintf(log_buff,MBUFF,"Connected to storage server %s:%d",packet->ip,packet->ns_port);
    log_into(log_buff);

    pthread_mutex_lock(&storage_cnt_lock);
    pthread_mutex_lock(&sockfd_lock);
    socket_fd[storage_count] = ss_fd;
    pthread_mutex_unlock(&sockfd_lock);
    pthread_mutex_unlock(&storage_cnt_lock);
#ifdef DEBUG
    printf("[DEBUG] socket_fd[%d] = %d\n", storage_count, ss_fd);
#endif

    // ping the storage server
    int ping_sock;
    if ((ping_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in ping_addr;
    memset(&ping_addr, '0', sizeof(ping_addr));
    ping_addr.sin_family = AF_INET;
    ping_addr.sin_port = htons(packet->ns_port);
    if (inet_pton(AF_INET, packet->ip, &ping_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        close(ping_sock);
        exit(EXIT_FAILURE);
    }
    cond = 0;
    for (int i = 0; i < 8; i++)
    {
        if (connect(ping_sock, (struct sockaddr *)&ping_addr, sizeof(ping_addr)) >= 0)
        {
            cond = 1;
            break;
        }
#ifdef DEBUG
        printf("[DEBUG] Connection failed, retrying...\n");
#endif
        sleep(1);
    }
    if (cond == 0)
    {
        perror("Connection failed");
        close(ss_fd);
        return;
    }
#ifdef DEBUG
    printf("[DEBUG] ping socket connected to SS, %d\n", ping_sock);
#endif
    stat = check_in_bef(packet->ip, packet->ns_port);

    #ifdef DEBUG
    printf("[DEBUG] stat = %d\n", stat);
    #endif

    if (stat == -1) {
        send_to_ping *ping_data = malloc(sizeof(send_to_ping));
        ping_data->ping_socket = ping_sock;
        pthread_mutex_lock(&storage_cnt_lock);
        ping_data->storage_id = storage_count;
        pthread_mutex_unlock(&storage_cnt_lock);
        pthread_t thread;
        pthread_create(&thread, NULL, start_pinging, ping_data);
        // pthread_detach(thread);
        #ifdef DEBUG
        printf("[DEBUG] paths = %d\n", packet->path_count);
        #endif
    }
    else {
        send_to_ping *ping_data = malloc(sizeof(send_to_ping));
        ping_data->ping_socket = ping_sock;
        ping_data->storage_id = stat;
        pthread_t thread;
        pthread_create(&thread, NULL, start_pinging, ping_data);
        // pthread_detach(thread);
        #ifdef DEBUG
        printf("[DEBUG] paths = %d\n", packet->path_count);
        #endif
    }

    if (stat != -1) {
        socket_fd[stat] = ss_fd;
        #ifdef DEBUG
        printf("[DEBUG] socket_fd[%d] = %d\n", stat, ss_fd);
        #endif
    }
    else {
        pthread_mutex_lock(&storage_cnt_lock);
        pthread_mutex_lock(&sockfd_lock);
        socket_fd[storage_count] = ss_fd;
        pthread_mutex_unlock(&sockfd_lock);
        pthread_mutex_unlock(&storage_cnt_lock);
        #ifdef DEBUG
        printf("[DEBUG] socket_fd[%d] = %d\n", storage_count, ss_fd);
        #endif
    }

    if (stat == -1) {
        for (int i = 0; i < packet->path_count; i++)
        {
            pthread_mutex_lock(&storage_cnt_lock);
            pthread_mutex_lock(&trie_lock);
            insert_path(packet->paths[i], storage_count);
            pthread_mutex_unlock(&trie_lock);
            pthread_mutex_unlock(&storage_cnt_lock);
        }
    }
    log_into("Connected to SS");

    send_to_ping *ping_data = malloc(sizeof(send_to_ping));
    ping_data->ping_socket = ping_sock;
    pthread_mutex_lock(&storage_cnt_lock);
    ping_data->storage_id = storage_count;
    pthread_mutex_unlock(&storage_cnt_lock);
    pthread_t thread;
    pthread_create(&thread, NULL, start_pinging, ping_data);
// pthread_detach(thread);
#ifdef DEBUG
    printf("[DEBUG] paths = %d\n", packet->path_count);
#endif


    for (int i = 0; i < packet->path_count; i++)
    {
        pthread_mutex_lock(&storage_cnt_lock);
        pthread_mutex_lock(&trie_lock);
        insert_path(packet->paths[i], storage_count);
        pthread_mutex_unlock(&trie_lock);
        pthread_mutex_unlock(&storage_cnt_lock);
    }

    if (stat == -1) {
        pthread_mutex_lock(&ss_packet_copy_lock);
        pthread_mutex_lock(&storage_cnt_lock);

        strcpy(storage_servers[storage_count].ip, packet->ip);
        storage_servers[storage_count].cl_port = packet->cl_port;
        storage_servers[storage_count].ns_port = packet->ns_port;
        storage_servers[storage_count].available = AVAILABLE;

        pthread_mutex_unlock(&storage_cnt_lock);
        pthread_mutex_unlock(&ss_packet_copy_lock);

        pthread_mutex_lock(&storage_cnt_lock);
        storage_count++;
        pthread_mutex_unlock(&storage_cnt_lock);
        #ifdef DEBUG
        printf("[DEUBG] count is %d\n", storage_count);
        #endif
    }

    // close(client_socket);
#ifdef DEBUG
    printf("[DEBUG] Storage server added successfully\n");
#endif
    log_into("Storage server added successfully");

    free(packet);
    if (stat != -1) {
        pthread_mutex_lock(&backup_lock);
        backup_node backup_ss = backup[stat];
        pthread_mutex_unlock(&backup_lock);
        int ddd = 0;
        if (storage_servers[backup_ss.bss1].available == AVAILABLE) send_to_leaves_modified(stat,backup_ss.bss1);
        else if (storage_servers[backup_ss.bss2].available == AVAILABLE) send_to_leaves_modified(stat,backup_ss.bss2);
        else ddd = 1;

        if (ddd == 0) return;
    }

    // just print in array
    for(int i = 0;i < storage_count;i++) {
        printf("Storage server %d: %s:%d\n", i, storage_servers[i].ip, storage_servers[i].cl_port);
    }
    // now we add backup SS if total count is >= 3
    int available_count = get_total_available();
    if (available_count >= 3)
    {
        pthread_mutex_lock(&bool_lock);
        if (first_time_done == false)
        {
            first_time_done = true;
            pthread_mutex_unlock(&bool_lock);
            pthread_mutex_lock(&ss_packet_copy_lock);
            pthread_mutex_lock(&storage_cnt_lock);

            for (int i = 0; i < storage_count; i++)
            {
                if (storage_servers[i].available == NOT_AVAILABLE)
                {
                    continue;
                }
            }

            for (int i = 0; i < storage_count; i++)
            {
                if (storage_servers[i].available == NOT_AVAILABLE)
                {
                    continue;
                }
                backup_node backup_ss = get_backup_ss(i);
                backup[i] = backup_ss;
                backup[i].bss1 = backup_ss.bss1;
                backup[i].bss2 = backup_ss.bss2;

                send_to_leaves(i, backup_ss.bss1);
                printf("\n");
                send_to_leaves(i, backup_ss.bss2);
                

                #ifdef DEBUG
                printf("----------------------------\n");
                printf("ssid : %d has bssid1: %d, bssid2: %d\n", i, backup[i].bss1, backup[i].bss2);
                printf("----------------------------\n");
                #endif

                printf("\n");
            }

            pthread_mutex_unlock(&storage_cnt_lock);
            pthread_mutex_unlock(&ss_packet_copy_lock);
            return;
        }
        pthread_mutex_unlock(&bool_lock);
        int backup_count = 0;
        pthread_mutex_lock(&storage_cnt_lock);
        pthread_mutex_lock(&backup_lock);
        pthread_mutex_lock(&ss_packet_copy_lock);
        backup_node backup_ss = get_backup_ss(storage_count - 1);
        pthread_mutex_unlock(&ss_packet_copy_lock);
        backup[storage_count - 1].bss1 = backup_ss.bss1;
        backup[storage_count - 1].bss2 = backup_ss.bss2;

        send_to_leaves(storage_count - 1, backup_ss.bss1);
        send_to_leaves(storage_count - 1, backup_ss.bss2);

#ifdef DEBUG
        printf("----------------------------\n");
        printf("ssid : %d has bssid1: %d, bssid2: %d\n", storage_count - 1, backup[storage_count - 1].bss1, backup[storage_count - 1].bss2);
        printf("----------------------------\n");
#endif

        pthread_mutex_unlock(&backup_lock);
        pthread_mutex_unlock(&storage_cnt_lock);
    }
}

void *get_server_data(void *args)
{
    while (1)
    {
        get_ss_data();
    }
    return NULL;
}
