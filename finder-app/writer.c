#include <stdio.h>
#include <syslog.h>

int main (int argc, char * argv[])
{
    openlog("writer", LOG_PID, LOG_USER);
    if (argc != 3 || !argv)
    {
        syslog(LOG_ERR, "Usage: writer <file_path> <str>");
        closelog();
        return 1;
    }

    char * filepath = argv[1];
    FILE * fp = fopen(filepath, "w");
    if (fp == NULL) 
    {
        syslog(LOG_ERR, "Cant open file %s", filepath);
        closelog();
        return 1;
    }
    char * str = argv[2];
    fprintf(fp, "%s\n", str);
    fclose(fp);

    syslog(LOG_DEBUG, "Writing to the file.");
    closelog();
    return 0;
}