#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#define W1_PATH "/sys/bus/w1/devices/"

// Find DS18B20 sensor (starts with "28-")
char* find_sensor() {
    DIR *dir;
    struct dirent *entry;
    static char sensor_path[256];
    
    dir = opendir(W1_PATH);
    if (!dir) {
        perror("Failed to open w1 devices directory");
        return NULL;
    }
    
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
    if (!fp) {
        perror("Failed to open sensor file");
        return temperature;
    }
    
    // Read first line (CRC check)
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return temperature;
    }
    
    // Check if CRC is valid (should end with "YES")
    if (strstr(line, "YES") == NULL) {
        fprintf(stderr, "CRC check failed\n");
        fclose(fp);
        return temperature;
    }
    
    // Read second line (contains temperature)
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return temperature;
    }
    
    // Find "t=" in the line
    temp_pos = strstr(line, "t=");
    if (temp_pos != NULL) {
        // Temperature is in millidegrees Celsius
        int temp_raw = atoi(temp_pos + 2);
        temperature = temp_raw / 1000.0;
    }
    
    fclose(fp);
    return temperature;
}

int main() {
    char *sensor_path;
    float temperature;
    
    printf("DS18B20 Temperature Reader\n");
    printf("==========================\n\n");
    
    // Find sensor
    sensor_path = find_sensor();
    if (sensor_path == NULL) {
        fprintf(stderr, "No DS18B20 sensor found!\n");
        fprintf(stderr, "Make sure:\n");
        fprintf(stderr, "  1. Sensor is connected properly\n");
        fprintf(stderr, "  2. dtoverlay=w1-gpio is in /boot/config.txt\n");
        fprintf(stderr, "  3. System has been rebooted\n");
        return 1;
    }
    
    printf("Sensor found: %s\n\n", sensor_path);
    
    // Read temperature continuously
    while (1) {
        temperature = read_temperature(sensor_path);
        
        if (temperature != -999.0) {
            printf("Temperature: %.2f°C (%.2f°F)\n", 
                   temperature, temperature * 9.0/5.0 + 32.0);
        } else {
            printf("Failed to read temperature\n");
        }
        
        sleep(2);  // Read every 2 seconds
    }
    
    return 0;
}