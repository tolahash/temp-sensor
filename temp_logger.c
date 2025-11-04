#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>

#define W1_PATH "/sys/bus/w1/devices/"
#define BUFFER_SIZE 100
#define LOG_FILE "temperature_log.csv"

// Circular buffer for temperature readings
typedef struct {
    float data[BUFFER_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} CircularBuffer;

CircularBuffer buffer;
volatile sig_atomic_t running = 1;

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    printf("\nReceived signal %d, shutting down...\n", signum);
    running = 0;
}

// Initialize circular buffer
void buffer_init(CircularBuffer *buf) {
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    pthread_mutex_init(&buf->mutex, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
    pthread_cond_init(&buf->not_full, NULL);
}

// Add temperature to buffer
void buffer_put(CircularBuffer *buf, float temp) {
    pthread_mutex_lock(&buf->mutex);
    
    while (buf->count == BUFFER_SIZE) {
        pthread_cond_wait(&buf->not_full, &buf->mutex);
    }
    
    buf->data[buf->head] = temp;
    buf->head = (buf->head + 1) % BUFFER_SIZE;
    buf->count++;
    
    pthread_cond_signal(&buf->not_empty);
    pthread_mutex_unlock(&buf->mutex);
}

// Get temperature from buffer
float buffer_get(CircularBuffer *buf) {
    float temp;
    
    pthread_mutex_lock(&buf->mutex);
    
    while (buf->count == 0 && running) {
        pthread_cond_wait(&buf->not_empty, &buf->mutex);
    }
    
    if (!running && buf->count == 0) {
        pthread_mutex_unlock(&buf->mutex);
        return -999.0;
    }
    
    temp = buf->data[buf->tail];
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;
    buf->count--;
    
    pthread_cond_signal(&buf->not_full);
    pthread_mutex_unlock(&buf->mutex);
    
    return temp;
}

// Find DS18B20 sensor
char* find_sensor() {
    DIR *dir;
    struct dirent *entry;
    static char sensor_path[256];
    
    dir = opendir(W1_PATH);
    if (!dir) return NULL;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "28-", 3) == 0) {
            snprintf(sensor_path, sizeof(sensor_path), 
                     "%s%s/w1_slave", W1_PATH, entry->d_name);
            closedir(dir);
            return sensor_path;
        }
    }
    
    closedir(dir);
    return NULL;
}

// Read temperature from sensor
float read_temperature(const char *sensor_path) {
    FILE *fp;
    char line[256];
    char *temp_pos;
    float temperature = -999.0;
    
    fp = fopen(sensor_path, "r");
    if (!fp) return temperature;
    
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return temperature;
    }
    
    if (strstr(line, "YES") == NULL) {
        fclose(fp);
        return temperature;
    }
    
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return temperature;
    }
    
    temp_pos = strstr(line, "t=");
    if (temp_pos != NULL) {
        int temp_raw = atoi(temp_pos + 2);
        temperature = temp_raw / 1000.0;
    }
    
    fclose(fp);
    return temperature;
}

// Sensor reading thread
void* sensor_thread(void *arg) {
    char *sensor_path = (char *)arg;
    float temperature;
    
    printf("Sensor thread started\n");
    
    while (running) {
        temperature = read_temperature(sensor_path);
        
        if (temperature != -999.0) {
            buffer_put(&buffer, temperature);
            printf("[SENSOR] Read: %.2f°C\n", temperature);
        } else {
            fprintf(stderr, "[SENSOR] Read failed\n");
        }
        
        sleep(2);  // Read every 2 seconds
    }
    
    printf("Sensor thread exiting\n");
    pthread_cond_signal(&buffer.not_empty);  // Wake up logger
    return NULL;
}

// Logger thread
void* logger_thread(void *arg) {
    FILE *fp;
    float temperature;
    time_t now;
    struct tm *timeinfo;
    char timestamp[64];
    
    printf("Logger thread started\n");
    
    // Open log file in append mode
    fp = fopen(LOG_FILE, "a");
    if (!fp) {
        perror("Failed to open log file");
        return NULL;
    }
    
    // Write header if file is empty
    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0) {
        fprintf(fp, "Timestamp,Temperature_C,Temperature_F\n");
    }
    
    while (running || buffer.count > 0) {
        temperature = buffer_get(&buffer);
        
        if (temperature == -999.0) break;
        
        // Get current timestamp
        time(&now);
        timeinfo = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        // Write to file
        fprintf(fp, "%s,%.2f,%.2f\n", 
                timestamp, temperature, temperature * 9.0/5.0 + 32.0);
        fflush(fp);  // Ensure data is written
        
        printf("[LOGGER] Logged: %s - %.2f°C\n", timestamp, temperature);
    }
    
    fclose(fp);
    printf("Logger thread exiting\n");
    return NULL;
}

int main() {
    char *sensor_path;
    pthread_t sensor_tid, logger_tid;
    
    printf("DS18B20 Multi-threaded Temperature Logger\n");
    printf("=========================================\n\n");
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Find sensor
    sensor_path = find_sensor();
    if (sensor_path == NULL) {
        fprintf(stderr, "No DS18B20 sensor found!\n");
        return 1;
    }
    
    printf("Sensor found: %s\n", sensor_path);
    printf("Logging to: %s\n", LOG_FILE);
    printf("Press Ctrl+C to stop\n\n");
    
    // Initialize buffer
    buffer_init(&buffer);
    
    // Create threads
    if (pthread_create(&sensor_tid, NULL, sensor_thread, sensor_path) != 0) {
        perror("Failed to create sensor thread");
        return 1;
    }
    
    if (pthread_create(&logger_tid, NULL, logger_thread, NULL) != 0) {
        perror("Failed to create logger thread");
        return 1;
    }
    
    // Wait for threads to finish
    pthread_join(sensor_tid, NULL);
    pthread_join(logger_tid, NULL);
    
    // Cleanup
    pthread_mutex_destroy(&buffer.mutex);
    pthread_cond_destroy(&buffer.not_empty);
    pthread_cond_destroy(&buffer.not_full);
    
    printf("\nShutdown complete. Check %s for logged data.\n", LOG_FILE);
    
    return 0;
}