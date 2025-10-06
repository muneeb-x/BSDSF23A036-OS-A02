/*
 * Programming Assignment 02: lsv1.0.0
 * This is the source file of version 1.0.0
 * Read the write-up of the assignment to add the features to this base version
 * Usage:
 *       $ lsv1.0.0 
 *       % lsv1.0.0  /home
 *       $ lsv1.0.0  /home/kali/   /etc/
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/types.h>
extern int errno;
// Function prototypes
void do_ls(const char *dir, int long_listing);
void mode_to_string(mode_t mode, char *str);
void format_time(time_t mtime, char *time_str);

int main(int argc, char const *argv[])
{
    int long_listing = 0;
    int opt;
    
    // Parse command line options
    while ((opt = getopt(argc, (char *const *)argv, "l")) != -1) {
        switch (opt) {
            case 'l':
                long_listing = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
                exit(1);
        }
    }
    
    // Adjust arguments after option parsing
    int non_opt_args = argc - optind;
    
    if (non_opt_args == 0) {
        do_ls(".", long_listing);
    } else {
        for (int i = optind; i < argc; i++) {
            printf("Directory listing of %s:\n", argv[i]);
            do_ls(argv[i], long_listing);
            puts("");
        }
    }
    return 0;
}

// Convert mode to permission string (e.g., -rw-r--r--)
void mode_to_string(mode_t mode, char *str) {
    str[0] = (S_ISDIR(mode)) ? 'd' : (S_ISLNK(mode)) ? 'l' : '-';
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';
    str[10] = '\0';
}

// Format time (remove newline from ctime)
void format_time(time_t mtime, char *time_str) {
    char *raw_time = ctime(&mtime);
    strncpy(time_str, raw_time + 4, 12); // Get "Mon DD HH:MM"
    time_str[12] = '\0';
}

void do_ls(const char *dir, int long_listing)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }
    
    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
            
        if (long_listing) {
            // Long listing format
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
            
            struct stat file_stat;
            if (stat(full_path, &file_stat) == -1) {
                perror("lstat");
                continue;
            }
            
            // Get file permissions
            char permissions[11];
            mode_to_string(file_stat.st_mode, permissions);
            
            // Get user and group names
            struct passwd *pw = getpwuid(file_stat.st_uid);
            struct group *gr = getgrgid(file_stat.st_gid);
            
            // Format time
            char time_str[13];
            format_time(file_stat.st_mtime, time_str);
            
            printf("%s %2lu %-8s %-8s %8ld %s %s\n", 
                   permissions,
                   file_stat.st_nlink,
                   pw ? pw->pw_name : "unknown",
                   gr ? gr->gr_name : "unknown", 
                   file_stat.st_size,
                   time_str,
                   entry->d_name);
        } else {
            // Simple format
            printf("%s\n", entry->d_name);
        }
    }
    
    if (errno != 0) {
        perror("readdir failed");
    }
    
    closedir(dp);
}
