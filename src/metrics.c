/**
 * @file metrics.c
 * @brief This file contains functions for retrieving system metrics such as memory usage, CPU usage, disk usage, CPU
 * temperature, and others.
 * @author 1v6n
 * @date 09/10/2024
 */

#include "metrics.h"

static const char* get_monitored_interface(void)
{
    const char* env_iface = getenv("NETWORK_INTERFACE");
    if (env_iface != NULL && env_iface[0] != '\0')
    {
        return env_iface;
    }

    static char detected_iface[64] = {0};
    if (detected_iface[0] != '\0')
    {
        return detected_iface;
    }

    FILE* fp = fopen(PROC_NET_DEV_PATH, "r");
    if (fp == NULL)
    {
        return NETWORK_INTERFACE;
    }

    char line[BUFFER_SIZE];
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char iface[64] = {0};
        if (sscanf(line, " %63[^:]:", iface) != 1)
        {
            continue;
        }

        if (strcmp(iface, "lo") != 0)
        {
            strncpy(detected_iface, iface, sizeof(detected_iface) - 1);
            break;
        }
    }

    fclose(fp);

    if (detected_iface[0] == '\0')
    {
        return NETWORK_INTERFACE;
    }

    return detected_iface;
}

static double read_value(const char* path)
{
    FILE* fp = fopen(path, "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        return RETURN_ERROR;
    }

    long long value;
    if (fscanf(fp, "%lld", &value) != 1)
    {
        fprintf(stderr, "Error reading value from %s\n", path);
        fclose(fp);
        return RETURN_ERROR;
    }

    fclose(fp);
    return value / UNIT_CONVERSION;
}

double get_memory_usage()
{
    FILE* fp = fopen(PROC_MEMINFO_PATH, "r");
    if (fp == NULL)
    {
        perror("Error opening " PROC_MEMINFO_PATH);
        return RETURN_ERROR;
    }

    char buffer[BUFFER_SIZE];
    unsigned long long total_mem_kb = 0, available_mem_kb = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        if (sscanf(buffer, "MemTotal: %llu kB", &total_mem_kb) == 1)
        {
            continue;
        }
        if (sscanf(buffer, "MemAvailable: %llu kB", &available_mem_kb) == 1)
        {
            break;
        }
    }

    fclose(fp);

    if (total_mem_kb == 0 || available_mem_kb == 0 || available_mem_kb > total_mem_kb)
    {
        fprintf(stderr, "Error reading memory information from " PROC_MEMINFO_PATH "\n");
        return RETURN_ERROR;
    }

    double used_mem_kb = (double)(total_mem_kb - available_mem_kb);
    double mem_usage_percent = (used_mem_kb / (double)total_mem_kb) * 100.0;

    return mem_usage_percent;
}

double get_cpu_usage()
{
    static unsigned long long prev_user = 0, prev_nice = 0, prev_system = 0, prev_idle = 0, prev_iowait = 0,
                              prev_irq = 0, prev_softirq = 0, prev_steal = 0;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

    FILE* fp = fopen(PROC_STAT_PATH, "r");
    if (fp == NULL)
    {
        perror("Error opening " PROC_STAT_PATH);
        return RETURN_ERROR;
    }

    char buffer[BUFFER_SIZE * 4];
    if (fgets(buffer, sizeof(buffer), fp) == NULL)
    {
        perror("Error reading " PROC_STAT_PATH);
        fclose(fp);
        return RETURN_ERROR;
    }
    fclose(fp);

    int ret = sscanf(buffer, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu", &user, &nice, &system, &idle, &iowait,
                     &irq, &softirq, &steal);
    if (ret < 8)
    {
        fprintf(stderr, "Error parsing " PROC_STAT_PATH "\n");
        return RETURN_ERROR;
    }

    unsigned long long prev_idle_total = prev_idle + prev_iowait;
    unsigned long long idle_total = idle + iowait;
    unsigned long long prev_non_idle = prev_user + prev_nice + prev_system + prev_irq + prev_softirq + prev_steal;
    unsigned long long non_idle = user + nice + system + irq + softirq + steal;
    unsigned long long prev_total = prev_idle_total + prev_non_idle;
    unsigned long long total = idle_total + non_idle;
    unsigned long long totald = total - prev_total;
    unsigned long long idled = idle_total - prev_idle_total;

    if (totald == 0)
    {
        fprintf(stderr, "Totald is zero, cannot calculate CPU usage!\n");
        return RETURN_ERROR;
    }

    double cpu_usage_percent = ((double)(totald - idled) / (double)totald) * 100.0;

    prev_user = user;
    prev_nice = nice;
    prev_system = system;
    prev_idle = idle;
    prev_iowait = iowait;
    prev_irq = irq;
    prev_softirq = softirq;
    prev_steal = steal;

    return cpu_usage_percent;
}

double get_disk_usage()
{
    struct statvfs stat;

    if (statvfs(ROOT_PATH, &stat) != 0)
    {
        fprintf(stderr, "Error getting file system statistics\n");
        return RETURN_ERROR;
    }

    unsigned long long total = (unsigned long long)stat.f_blocks * stat.f_frsize;
    unsigned long long available = (unsigned long long)stat.f_bavail * stat.f_frsize;
    unsigned long long used = total - available;

    if (total == 0)
    {
        fprintf(stderr, "Invalid total disk size\n");
        return RETURN_ERROR;
    }

    double usage_percentage = ((double)used / (double)total) * PERCENTAGE;
    return usage_percentage;
}

double get_cpu_temperature()
{
    return read_value(HWMON_CPU_TEMP_PATH);
}

double get_battery_voltage()
{
    return read_value(HWMON_BATTERY_VOLTAGE_PATH);
}

double get_battery_current()
{
    return read_value(HWMON_BATTERY_CURRENT_PATH);
}

double get_cpu_frequency()
{
    return read_value(CPU_FREQ_PATH);
}

double get_cpu_fan_speed()
{
    return read_value(CPU_FAN_SPEED_PATH) * UNIT_CONVERSION;
}

double get_gpu_fan_speed()
{
    return read_value(GPU_FAN_SPEED_PATH) * UNIT_CONVERSION;
}

void get_process_states(int* total, int* suspended, int* ready, int* blocked)
{
    DIR* proc_dir = opendir(PROC_DIR_PATH);
    if (proc_dir == NULL)
    {
        perror("Error opening " PROC_DIR_PATH);
        return;
    }

    struct dirent* entry;
    *total = 0;
    *suspended = 0;
    *ready = 0;
    *blocked = 0;

    while ((entry = readdir(proc_dir)) != NULL)
    {
        if (isdigit(entry->d_name[0]))
        {
            char path[BUFFER_SIZE];
            snprintf(path, sizeof(path), STAT_FILE_FORMAT, entry->d_name);

            FILE* fp = fopen(path, "r");
            if (fp == NULL)
            {
                continue;
            }

            char state;
            if (fscanf(fp, "%*d %*s %c", &state) == 1)
            {
                (*total)++;
                if (state == 'S')
                {
                    (*suspended)++;
                }
                else if (state == 'R')
                {
                    (*ready)++;
                }
                else if (state == 'D')
                {
                    (*blocked)++;
                }
            }

            fclose(fp);
        }
    }

    closedir(proc_dir);
}

double get_total_memory()
{
    FILE* fp = fopen(PROC_MEMINFO_PATH, "r");
    if (fp == NULL)
    {
        perror("Error opening " PROC_MEMINFO_PATH);
        return RETURN_ERROR;
    }

    char buffer[BUFFER_SIZE];
    unsigned long long total_mem_kb = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        if (sscanf(buffer, "MemTotal: %llu kB", &total_mem_kb) == 1)
        {
            break;
        }
    }

    fclose(fp);
    return ((double)total_mem_kb) / CONVERT_TO_MB;
}

double get_used_memory()
{
    FILE* fp = fopen(PROC_MEMINFO_PATH, "r");
    if (fp == NULL)
    {
        perror("Error opening " PROC_MEMINFO_PATH);
        return RETURN_ERROR;
    }

    char buffer[BUFFER_SIZE];
    unsigned long long total_mem_kb = 0, free_mem_kb = 0, buffers_kb = 0, cached_kb = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        sscanf(buffer, "MemTotal: %llu kB", &total_mem_kb);
        sscanf(buffer, "MemFree: %llu kB", &free_mem_kb);
        sscanf(buffer, "Buffers: %llu kB", &buffers_kb);
        sscanf(buffer, "Cached: %llu kB", &cached_kb);
    }

    fclose(fp);
    if (total_mem_kb == 0)
    {
        fprintf(stderr, "Error reading memory information from " PROC_MEMINFO_PATH "\n");
        return RETURN_ERROR;
    }

    unsigned long long reclaimable_kb = free_mem_kb + buffers_kb + cached_kb;
    if (reclaimable_kb > total_mem_kb)
    {
        reclaimable_kb = total_mem_kb;
    }

    return ((double)(total_mem_kb - reclaimable_kb)) / CONVERT_TO_MB;
}

double get_available_memory()
{
    FILE* fp = fopen(PROC_MEMINFO_PATH, "r");
    if (fp == NULL)
    {
        perror("Error opening " PROC_MEMINFO_PATH);
        return RETURN_ERROR;
    }

    char buffer[BUFFER_SIZE];
    unsigned long long available_mem_kb = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        if (sscanf(buffer, "MemAvailable: %llu kB", &available_mem_kb) == 1)
        {
            break;
        }
    }

    fclose(fp);
    return ((double)available_mem_kb) / CONVERT_TO_MB;
}

NetworkStats get_network_traffic()
{
    FILE* fp = fopen(PROC_NET_DEV_PATH, "r");
    if (fp == NULL)
    {
        perror("Error opening " PROC_NET_DEV_PATH);
        return (NetworkStats){RETURN_ERROR, RETURN_ERROR, RETURN_ERROR, RETURN_ERROR, RETURN_ERROR};
    }

    char buffer[BUFFER_SIZE];
    unsigned long long rx_bytes = 0, tx_bytes = 0;
    unsigned long long rx_errors = 0, tx_errors = 0, dropped_packets = 0;
    const char* monitored_iface = get_monitored_interface();

    fgets(buffer, sizeof(buffer), fp);
    fgets(buffer, sizeof(buffer), fp);

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        char iface[64] = {0};
        unsigned long long r_bytes, t_bytes, r_errors, t_errors, drop;

        int matched = sscanf(buffer, " %63[^:]: %llu %*d %llu %llu %*d %*d %*d %*d %llu %*d %llu", iface, &r_bytes,
                             &r_errors, &drop, &t_bytes, &t_errors);

        if (matched != 6)
        {
            continue;
        }

        if (strcmp(iface, monitored_iface) == 0)
        {
            rx_bytes = r_bytes;
            tx_bytes = t_bytes;
            rx_errors = r_errors;
            tx_errors = t_errors;
            dropped_packets = drop;
            break;
        }
    }

    fclose(fp);
    return (NetworkStats){rx_bytes, tx_bytes, rx_errors, tx_errors, dropped_packets};
}

unsigned long long get_context_switches()
{
    FILE* fp = fopen(PROC_STAT_PATH, "r");
    if (fp == NULL)
    {
        perror("Error opening /proc/stat");
        return 0;
    }

    char buffer[BUFFER_SIZE];
    unsigned long long context_switches = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        if (sscanf(buffer, "ctxt %llu", &context_switches) == 1)
        {
            break;
        }
    }

    fclose(fp);
    return context_switches;
}

DiskStats get_disk_stats()
{
    FILE* fp = fopen(DISKSTATS_PATH, "r");
    if (fp == NULL)
    {
        perror("Error opening " DISKSTATS_PATH);
        return (DiskStats){RETURN_ERROR, RETURN_ERROR, RETURN_ERROR};
    }

    char buffer[BUFFER_SIZE];
    unsigned long long io_time = 0, writes_completed = 0, reads_completed = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        unsigned long long it, wc, rc;
        if (sscanf(buffer, "%*d %*d %*s %llu %*d %*d %*d %llu %*d %llu", &rc, &wc, &it) == 3)
        {
            reads_completed += rc;
            writes_completed += wc;
            io_time += it;
        }
    }

    fclose(fp);
    return (DiskStats){io_time, writes_completed, reads_completed};
}
