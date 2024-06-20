#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#define PATH_MAX 4096 // PATH_MAX sabitini burada tanımlayabilirsiniz.
#define BUFSIZE 4096 // Increased buffer size

// Structure to hold file descriptors
typedef struct {
    int src_fd;
    int dst_fd;
    char src_name[PATH_MAX];
    char dst_name[PATH_MAX];
} FileInformation;

// Buffer structure
typedef struct {
    FileInformation *data;
    int size;
    int count;
    int head;
    int tail;
} Buffer;

Buffer buffer;
pthread_mutex_t buffer_mutex;
pthread_cond_t buffer_not_full;
pthread_cond_t buffer_not_empty;
pthread_barrier_t worker_barrier; // Barrier declaration
int done = 0;
int num_files_copied = 0;
int num_directories = 0;
int num_fifo_files = 0;
long long total_bytes_copied = 0;
struct timeval start_time, end_time;
int num_workers;

pthread_t *worker_threads;

// Function prototypes
void print_usage_and_exit();

//BUFFER FUNCTIONS
void initialize_buffer(int size);
int buffer_is_empty();
int buffer_is_full();
void buffer_add(int src_fd, int dst_fd, const char *src_name, const char *dst_name);
FileInformation buffer_get();

//THREAD FUNCTIONS
void *manager_function(void *arg);
void *worker_function(void *arg);

//HELPER FUNCTIONS
void traverse_directory(const char *src_dir, const char *dst_dir);
void copy_file_contents(int src_fd, int dst_fd);
void cleanup();
void print_statistics(int num_workers, int buffer_size);
void signal_handler(int signal);
void handle_error(const char *message);

int main(int argc, char *argv[]) {
    if (argc != 5) {
        print_usage_and_exit(); // Print usage and exit if arguments are not correct
    }

    int buffer_size = atoi(argv[1]); // Convert buffer size to integer
    num_workers = atoi(argv[2]); // Convert number of workers to integer
    char *src_dir = argv[3]; // Source directory
    char *dst_dir = argv[4]; // Destination directory

    // Initialize buffer, mutex, condition variables, and barrier
    initialize_buffer(buffer_size); 
    pthread_mutex_init(&buffer_mutex, NULL);
    pthread_cond_init(&buffer_not_full, NULL);
    pthread_cond_init(&buffer_not_empty, NULL);
    pthread_barrier_init(&worker_barrier, NULL, num_workers + 1); // +1 for the manager thread

    // Set up signal handler for graceful termination
    signal(SIGINT, signal_handler);

    // Start timing
    gettimeofday(&start_time, NULL);

    // Start manager thread
    pthread_t manager_thread;
    char *src_dst_dirs[] = {src_dir, dst_dir};
    pthread_create(&manager_thread, NULL, manager_function, (void *)src_dst_dirs);

    // Start worker threads
    worker_threads = malloc(num_workers * sizeof(pthread_t));
    for (int i = 0; i < num_workers; ++i) {
        pthread_create(&worker_threads[i], NULL, worker_function, NULL);
    }

    // Wait for manager thread to complete
    pthread_join(manager_thread, NULL);

    // Wait for worker threads to complete
    for (int i = 0; i < num_workers; ++i) {
        pthread_join(worker_threads[i], NULL);
    }

    // Stop timing
    gettimeofday(&end_time, NULL);

    // Cleanup and print statistics
    cleanup();
    print_statistics(num_workers, buffer_size);

    return 0;
}

void print_usage_and_exit() {
    fprintf(stderr, "Usage: ./MWCp <buffer_size> <num_workers> <src_dir> <dst_dir>\n");
    exit(1);
}

void initialize_buffer(int size) {
    buffer.size = size;
    buffer.data = malloc(size * sizeof(FileInformation));
    if (buffer.data == NULL) {
        handle_error("Failed to allocate buffer");
    }
    buffer.count = 0;
    buffer.head = 0;
    buffer.tail = 0;
}

int buffer_is_empty() {
    return buffer.count == 0;
}

int buffer_is_full() {
    return buffer.count == buffer.size;
}

void buffer_add(int src_fd, int dst_fd, const char *src_name, const char *dst_name) {
    buffer.data[buffer.tail].src_fd = src_fd;
    buffer.data[buffer.tail].dst_fd = dst_fd;
    strncpy(buffer.data[buffer.tail].src_name, src_name, PATH_MAX);
    strncpy(buffer.data[buffer.tail].dst_name, dst_name, PATH_MAX);
    buffer.tail = (buffer.tail + 1) % buffer.size;
    buffer.count++;
}

FileInformation buffer_get() {
    FileInformation fd_pair = buffer.data[buffer.head];
    buffer.head = (buffer.head + 1) % buffer.size;
    buffer.count--;
    return fd_pair;
}

void *manager_function(void *arg) {
    char **src_dst_dirs = (char **)arg;
    char *src_dir = src_dst_dirs[0];
    char *dst_dir = src_dst_dirs[1];

    traverse_directory(src_dir, dst_dir);

    pthread_mutex_lock(&buffer_mutex);
    done = 1;
    pthread_cond_broadcast(&buffer_not_empty);
    pthread_mutex_unlock(&buffer_mutex);

    pthread_barrier_wait(&worker_barrier); // Wait at the barrier before finishing

    return NULL;
}

void traverse_directory(const char *src_dir, const char *dst_dir) {
    DIR *dir;
    struct dirent *entry;
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];

    if ((dir = opendir(src_dir)) == NULL) {
        handle_error("Failed to open source directory");
    }

    if (mkdir(dst_dir, 0755) == -1 && errno != EEXIST) {
        closedir(dir);
        handle_error("Failed to create destination directory");
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, PATH_MAX, "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, PATH_MAX, "%s/%s", dst_dir, entry->d_name);

        struct stat statbuf;
        if (stat(src_path, &statbuf) == -1) {
            fprintf(stderr, "Failed to stat %s: %s\n", src_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            num_directories++;
            traverse_directory(src_path, dst_path);
        } else if (S_ISREG(statbuf.st_mode)) {
            int src_fd = open(src_path, O_RDONLY);
            if (src_fd == -1) {
                fprintf(stderr, "Failed to open source file %s: %s\n", src_path, strerror(errno));
                continue;
            }

            int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dst_fd == -1) {
                fprintf(stderr, "Failed to open destination file %s: %s\n", dst_path, strerror(errno));
                close(src_fd);
                continue;
            }

            pthread_mutex_lock(&buffer_mutex);
            while (buffer_is_full() && !done) {
                pthread_cond_wait(&buffer_not_full, &buffer_mutex);
            }
            if (done) {
                pthread_mutex_unlock(&buffer_mutex);
                close(src_fd);
                close(dst_fd);
                break;
            }
            buffer_add(src_fd, dst_fd, src_path, dst_path);
            pthread_cond_signal(&buffer_not_empty);
            pthread_mutex_unlock(&buffer_mutex);
        } else if (S_ISFIFO(statbuf.st_mode)) {
            num_fifo_files++;
            if (mkfifo(dst_path, 0666) == -1) {
                fprintf(stderr, "Failed to create FIFO at %s: %s\n", dst_path, strerror(errno));
            }
        }
    }

    closedir(dir);
}


void *worker_function(void *arg) {
    while (1) {
        pthread_mutex_lock(&buffer_mutex);
        while (buffer_is_empty() && !done) {
            pthread_cond_wait(&buffer_not_empty, &buffer_mutex);
        }

        if (buffer_is_empty() && done) {
            pthread_mutex_unlock(&buffer_mutex);
            break;
        }

        FileInformation fd_pair = buffer_get();
        pthread_cond_signal(&buffer_not_full);
        pthread_mutex_unlock(&buffer_mutex);

        copy_file_contents(fd_pair.src_fd, fd_pair.dst_fd);
        close(fd_pair.src_fd);
        close(fd_pair.dst_fd);

        // Update counters with mutex lock
        pthread_mutex_lock(&buffer_mutex);
        num_files_copied++;
        pthread_mutex_unlock(&buffer_mutex);
    }

    pthread_barrier_wait(&worker_barrier); // Wait at the barrier before finishing

    return NULL;
}

void copy_file_contents(int src_fd, int dst_fd) {
    char buffer[BUFSIZE];
    ssize_t nread;
    while ((nread = read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (write(dst_fd, buffer, nread) != nread) {
            handle_error("Failed to write to destination file");
        }
        // Update total bytes copied with mutex lock
        pthread_mutex_lock(&buffer_mutex);
        total_bytes_copied += nread;
        pthread_mutex_unlock(&buffer_mutex);
    }
}

void cleanup() {
    free(buffer.data);
    free(worker_threads);
    pthread_mutex_destroy(&buffer_mutex);
    pthread_cond_destroy(&buffer_not_full);
    pthread_cond_destroy(&buffer_not_empty);
    pthread_barrier_destroy(&worker_barrier); // Destroy the barrier
}

void print_statistics(int num_workers, int buffer_size) {
    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1e6;
    printf("---------------STATISTICS--------------------\n");
    printf("Consumers: %d - Buffer Size: %d\n", num_workers, buffer_size);
    printf("Number of Regular Files: %d\n", num_files_copied);
    printf("Number of Directories: %d\n", num_directories);
    printf("Number of FIFO Files: %d\n", num_fifo_files);
    printf("TOTAL BYTES COPIED: %lld\n", total_bytes_copied);
    printf("TOTAL TIME: %.3f seconds\n", elapsed_time);
}

void signal_handler(int signal) {
    if (signal == SIGINT) {
        fprintf(stderr, "\nTermination signal received. Cleaning up and exiting...\n");

        pthread_mutex_lock(&buffer_mutex);
        done = 1;
        pthread_cond_broadcast(&buffer_not_empty);
        pthread_mutex_unlock(&buffer_mutex);

        // Wait for worker threads to finish
        for (int i = 0; i < num_workers; ++i) {
            pthread_join(worker_threads[i], NULL);
        }

        // Stop timing
        gettimeofday(&end_time, NULL);

        cleanup();
        print_statistics(num_workers, buffer.size);
        exit(0);
    }
}

void handle_error(const char *message) {
    perror(message);
    exit(1);
}
