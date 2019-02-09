//Last modified: 18/11/12 19:13:35(CET) by Fabian Holler
#include <stdlib.h> 
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
    return milliseconds;
}


struct pstat {
    long unsigned int utime_ticks;
    long int cutime_ticks;
    long unsigned int stime_ticks;
    long int cstime_ticks;
    long unsigned int vsize; // virtual memory size in bytes
    long unsigned int rss; //Resident  Set  Size in bytes

    long unsigned int cpu_total_time;
};

/*
 * read /proc data into the passed struct pstat
 * returns 0 on success, -1 on error
*/
int get_usage(const pid_t pid, struct pstat* result) {
    //convert  pid to string
    char pid_s[20];
    snprintf(pid_s, sizeof(pid_s), "%d", pid);
    char stat_filepath[30] = "/proc/"; strncat(stat_filepath, pid_s,
            sizeof(stat_filepath) - strlen(stat_filepath) -1);
    strncat(stat_filepath, "/stat", sizeof(stat_filepath) -
            strlen(stat_filepath) -1);

    FILE *fpstat = fopen(stat_filepath, "r");
    if (fpstat == NULL) {
        perror("FOPEN ERROR ");
        return -1;
    }

    FILE *fstat = fopen("/proc/stat", "r");
    if (fstat == NULL) {
        perror("FOPEN ERROR ");
        fclose(fstat);
        return -1;
    }

    //read values from /proc/pid/stat
    bzero(result, sizeof(struct pstat));
    long int rss;
    if (fscanf(fpstat, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu"
                "%lu %ld %ld %*d %*d %*d %*d %*u %lu %ld",
                &result->utime_ticks, &result->stime_ticks,
                &result->cutime_ticks, &result->cstime_ticks, &result->vsize,
                &rss) == EOF) {
        fclose(fpstat);
        return -1;
    }
    fclose(fpstat);
    result->rss = rss * getpagesize();

    //read+calc cpu total time from /proc/stat
    long unsigned int cpu_time[10];
    bzero(cpu_time, sizeof(cpu_time));
    if (fscanf(fstat, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                &cpu_time[0], &cpu_time[1], &cpu_time[2], &cpu_time[3],
                &cpu_time[4], &cpu_time[5], &cpu_time[6], &cpu_time[7],
                &cpu_time[8], &cpu_time[9]) == EOF) {
        fclose(fstat);
        return -1;
    }

    fclose(fstat);

    for(int i=0; i < 10;i++)
        result->cpu_total_time += cpu_time[i];

    return 0;
}

/*
* calculates the elapsed CPU usage between 2 measuring points. in percent
*/
void calc_cpu_usage_pct(const struct pstat* cur_usage,
                        const struct pstat* last_usage,
                        double* ucpu_usage, double* scpu_usage)
{
    const long unsigned int total_time_diff = cur_usage->cpu_total_time -
                                              last_usage->cpu_total_time;

    *ucpu_usage = 100 * (((cur_usage->utime_ticks + cur_usage->cutime_ticks)
                    - (last_usage->utime_ticks + last_usage->cutime_ticks))
                    / (double) total_time_diff);

    *scpu_usage = 100 * ((((cur_usage->stime_ticks + cur_usage->cstime_ticks)
                    - (last_usage->stime_ticks + last_usage->cstime_ticks))) /
                    (double) total_time_diff);
}

/*
* calculates the elapsed CPU usage between 2 measuring points in ticks
*/
void calc_cpu_usage(const struct pstat* cur_usage,
                    const struct pstat* last_usage,
                    long unsigned int* ucpu_usage,
                    long unsigned int* scpu_usage)
{
    *ucpu_usage = (cur_usage->utime_ticks + cur_usage->cutime_ticks) -
                  (last_usage->utime_ticks + last_usage->cutime_ticks);

    *scpu_usage = (cur_usage->stime_ticks + cur_usage->cstime_ticks) -
                  (last_usage->stime_ticks + last_usage->cstime_ticks);
}


int main()
{
    // sample for 500 ms
    struct timespec tm;
    tm.tv_sec  = 0;
    tm.tv_nsec = 500 * 1000 * 1000; /* 500 ms */
    struct pstat first;
    struct pstat second;
    double cpu_usage, scpu_usage;
    int nb = sysconf(_SC_NPROCESSORS_ONLN);
    struct timeval tp;
    char pidline[1024];
    char *pid;
    int i =0;
    int pidno[64];
    FILE *fp = popen("pgrep -f youtube_test","r");
    if (fp == NULL) {
	printf("Failed to run command\n" );
	exit(1);
    }
    fgets(pidline,1024,fp);
    
    printf("%s",pidline);
    pid = strtok (pidline," ");
    while(pid != NULL)
    {
    
        pidno[i] = atoi(pid);
        //printf("%d\n",pidno[i]);
        pid = strtok (NULL , " ");
        i++;
    }
    //printf("%d\n",pidno[0]);
    pclose(fp);

    while(1)
    {
        if(get_usage(pidno[0], &first) == -1)
		break;
        nanosleep(&tm, NULL);
        if(get_usage(pidno[0], &second) == -1)
		break;
        calc_cpu_usage_pct(&second, &first, &cpu_usage, &scpu_usage);
        //printf("%lld,%f,%f\n", current_timestamp(), cpu_usage*nb, scpu_usage*nb);
        printf("%lld,%f\n", current_timestamp(), (cpu_usage+scpu_usage)*nb);
    }
    
	return 0;
}
