/*
 * Programming Assignment 02: lsv1.0.0
 * Updated for Feature 7: Recursive Listing
 * Features: Basic ls, Long format (-l), Column display, Horizontal display (-x), 
 *           Alphabetical sort, Colors, Recursive listing (-R)
 */

#define _GNU_SOURCE
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
#include <sys/ioctl.h>

extern int errno;

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_PINK    "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_REVERSE "\033[7m"

// Function prototypes - UPDATED FOR FEATURE 7
void do_ls(const char *dir, int long_listing, int horizontal_display, int recursive);
void do_ls_recursive(const char *dir, int long_listing, int horizontal_display);
void mode_to_string(mode_t mode, char *str);
void format_time(time_t mtime, char *time_str);
void display_columns(char **files, int count, int terminal_width);
void display_horizontal(char **files, int count, int terminal_width);
int compare_strings(const void *a, const void *b);
const char *get_file_color(const char *filename, mode_t mode);

// Comparison function for qsort
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

// Determine color based on file type and permissions
const char *get_file_color(const char *filename, mode_t mode) {
    if (S_ISDIR(mode)) {
        return COLOR_BLUE;           // Directories: Blue
    }
    else if (S_ISLNK(mode)) {
        return COLOR_PINK;           // Symbolic links: Pink
    }
    else if (S_ISCHR(mode) || S_ISBLK(mode)) {
        return COLOR_REVERSE;        // Device files: Reverse video
    }
    else if (mode & S_IXUSR) {
        return COLOR_GREEN;          // Executables: Green
    }
    else {
        // Check for specific file extensions
        const char *ext = strrchr(filename, '.');
        if (ext) {
            if (strcmp(ext, ".tar") == 0 || strcmp(ext, ".gz") == 0 || 
                strcmp(ext, ".zip") == 0 || strcmp(ext, ".tgz") == 0 ||
                strcmp(ext, ".bz2") == 0 || strcmp(ext, ".xz") == 0) {
                return COLOR_RED;    // Archives: Red
            }
            else if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 ||
                     strcmp(ext, ".cpp") == 0 || strcmp(ext, ".py") == 0) {
                return COLOR_CYAN;   // Source code: Cyan
            }
        }
    }
    return COLOR_RESET;              // Regular files: Default
}

// NEW FOR FEATURE 7: Main function with -R option
int main(int argc, char const *argv[])
{
    int long_listing = 0;
    int horizontal_display = 0;
    int recursive = 0;  // NEW: Recursive flag
    int opt;
    
    // Parse command line options - UPDATED FOR FEATURE 7
    while ((opt = getopt(argc, (char *const *)argv, "lxR")) != -1) {
        switch (opt) {
            case 'l':
                long_listing = 1;
                break;
            case 'x':
                horizontal_display = 1;
                break;
            case 'R':  // NEW: Recursive option
                recursive = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [-R] [directory...]\n", argv[0]);
                exit(1);
        }
    }
    
    // Adjust arguments after option parsing
    int non_opt_args = argc - optind;
    
    if (non_opt_args == 0) {
        if (recursive) {
            do_ls_recursive(".", long_listing, horizontal_display);
        } else {
            do_ls(".", long_listing, horizontal_display, 0);
        }
    } else {
        for (int i = optind; i < argc; i++) {
            if (recursive) {
                do_ls_recursive(argv[i], long_listing, horizontal_display);
            } else {
                printf("Directory listing of %s:\n", argv[i]);
                do_ls(argv[i], long_listing, horizontal_display, 0);
                if (i < argc - 1) puts("");
            }
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

// Display files in column format (down then across)
void display_columns(char **files, int count, int terminal_width) {
    if (count == 0) return;
    
    // Find the longest filename length
    int max_len = 0;
    for (int i = 0; i < count; i++) {
        int len = strlen(files[i]);
        if (len > max_len) max_len = len;
    }
    
    // Calculate column layout
    int col_width = max_len + 2; // Add 2 spaces between columns
    int num_cols = terminal_width / col_width;
    if (num_cols == 0) num_cols = 1; // At least 1 column
    
    int num_rows = (count + num_cols - 1) / num_cols; // Ceiling division
    
    // Print in "down then across" order
    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            int index = row + col * num_rows;
            if (index < count) {
                printf("%-*s", col_width, files[index]);
            }
        }
        printf("\n");
    }
}

// Display files in horizontal format (left to right)
void display_horizontal(char **files, int count, int terminal_width) {
    if (count == 0) return;
    
    // Find the longest filename length
    int max_len = 0;
    for (int i = 0; i < count; i++) {
        int len = strlen(files[i]);
        if (len > max_len) max_len = len;
    }
    
    int col_width = max_len + 2; // Add 2 spaces between columns
    int current_width = 0;
    
    for (int i = 0; i < count; i++) {
        int needed_width = strlen(files[i]) + 2;
        
        // Check if we need a new line
        if (current_width + needed_width > terminal_width && current_width > 0) {
            printf("\n");
            current_width = 0;
        }
        
        printf("%-*s", col_width, files[i]);
        current_width += col_width;
    }
    
    if (count > 0) printf("\n");
}

// NEW FOR FEATURE 7: Recursive directory listing function
void do_ls_recursive(const char *dir, int long_listing, int horizontal_display) {
    printf("\n%s:\n", dir);
    do_ls(dir, long_listing, horizontal_display, 1);  // Pass recursive=1
}

// MAIN do_ls FUNCTION with ALL features integrated - UPDATED FOR FEATURE 7
void do_ls(const char *dir, int long_listing, int horizontal_display, int recursive)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    int count = 0;  // Count non-hidden files
    
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }
    
    // Arrays to store file info for color display
    char **files = NULL;
    char **colors = NULL;
    mode_t *modes = NULL;
    int *is_directory = NULL;  // NEW: Track which files are directories
    
    // First pass: collect all file information
    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
            
        count++;
        
        if (long_listing) {
            // Long listing format with colors
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
            
            struct stat file_stat;
            if (stat(full_path, &file_stat) == -1) {
                perror("stat");
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
            
            // Get color for the file
            const char *color = get_file_color(entry->d_name, file_stat.st_mode);
            
            printf("%s %2lu %-8s %-8s %8ld %s %s%s%s\n", 
                   permissions,
                   file_stat.st_nlink,
                   pw ? pw->pw_name : "unknown",
                   gr ? gr->gr_name : "unknown", 
                   file_stat.st_size,
                   time_str,
                   color,  // Color start
                   entry->d_name,
                   COLOR_RESET);  // Color reset
        }
    }

    // Handle column/horizontal display after reading all entries
    if (!long_listing) {
        // Allocate memory for file info
        files = malloc(count * sizeof(char *));
        colors = malloc(count * sizeof(char *));
        modes = malloc(count * sizeof(mode_t));
        is_directory = malloc(count * sizeof(int));  // NEW: For recursive
        
        if (!files || !colors || !modes || !is_directory) {
            perror("malloc failed");
            if (files) free(files);
            if (colors) free(colors);
            if (modes) free(modes);
            if (is_directory) free(is_directory);
            closedir(dp);
            return;
        }
        
        // Get terminal width
        struct winsize w;
        int terminal_width = 80; // Default fallback
        
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) {
            terminal_width = w.ws_col;
        }
        
        // Reset and read directory again to collect filenames with metadata
        rewinddir(dp);
        int index = 0;
        errno = 0;
        while ((entry = readdir(dp)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            
            // Get file metadata for color determination
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
            struct stat file_stat;
            
            if (stat(full_path, &file_stat) == -1) {
                perror("stat");
                continue;
            }
            
            // Store filename
            files[index] = malloc(strlen(entry->d_name) + 1);
            if (!files[index]) {
                perror("malloc failed");
                closedir(dp);
                return;
            }
            strcpy(files[index], entry->d_name);
            
            // Store file mode for color
            modes[index] = file_stat.st_mode;
            
            // NEW FOR FEATURE 7: Track if this is a directory
            is_directory[index] = S_ISDIR(file_stat.st_mode);
            
            // Determine and store color
            const char *color_code = get_file_color(entry->d_name, file_stat.st_mode);
            colors[index] = malloc(strlen(color_code) + 1);
            if (!colors[index]) {
                perror("malloc failed");
                closedir(dp);
                return;
            }
            strcpy(colors[index], color_code);
            
            index++;
        }
        
        // Sort the files alphabetically
        qsort(files, count, sizeof(char *), compare_strings);
        
        // Choose display mode with colors
        if (horizontal_display) {
            // Display with colors in horizontal format
            int max_len = 0;
            for (int i = 0; i < count; i++) {
                int len = strlen(files[i]);
                if (len > max_len) max_len = len;
            }
            
            int col_width = max_len + 2;
            int current_width = 0;
            
            for (int i = 0; i < count; i++) {
                int needed_width = strlen(files[i]) + 2;
                
                if (current_width + needed_width > terminal_width && current_width > 0) {
                    printf("\n");
                    current_width = 0;
                }
                
                // Find the color for this file
                const char *color = COLOR_RESET;
                for (int j = 0; j < count; j++) {
                    if (strcmp(files[i], files[j]) == 0) {
                        color = colors[j];
                        break;
                    }
                }
                
                printf("%s%-*s%s", color, col_width, files[i], COLOR_RESET);
                current_width += col_width;
            }
            if (count > 0) printf("\n");
        } else {
            // Display with colors in column format
            int max_len = 0;
            for (int i = 0; i < count; i++) {
                int len = strlen(files[i]);
                if (len > max_len) max_len = len;
            }
            
            int col_width = max_len + 2;
            int num_cols = terminal_width / col_width;
            if (num_cols == 0) num_cols = 1;
            
            int num_rows = (count + num_cols - 1) / num_cols;
            
            for (int row = 0; row < num_rows; row++) {
                for (int col = 0; col < num_cols; col++) {
                    int index = row + col * num_rows;
                    if (index < count) {
                        // Find the color for this file
                        const char *color = COLOR_RESET;
                        for (int j = 0; j < count; j++) {
                            if (strcmp(files[index], files[j]) == 0) {
                                color = colors[j];
                                break;
                            }
                        }
                        printf("%s%-*s%s", color, col_width, files[index], COLOR_RESET);
                    }
                }
                printf("\n");
            }
        }
        
        // NEW FOR FEATURE 7: Recursive directory traversal
        if (recursive) {
            for (int i = 0; i < count; i++) {
                // Find if this file is a directory
                int dir_flag = 0;
                for (int j = 0; j < count; j++) {
                    if (strcmp(files[i], files[j]) == 0) {
                        dir_flag = is_directory[j];
                        break;
                    }
                }
                
                // Skip . and .. directories to avoid infinite recursion
                if (dir_flag && strcmp(files[i], ".") != 0 && strcmp(files[i], "..") != 0) {
                    // Build full path for recursive call
                    char full_path[1024];
                    snprintf(full_path, sizeof(full_path), "%s/%s", dir, files[i]);
                    
                    // Recursive call
                    do_ls_recursive(full_path, long_listing, horizontal_display);
                }
            }
        }
        
        // Cleanup
        for (int i = 0; i < count; i++) {
            free(files[i]);
            free(colors[i]);
        }
        free(files);
        free(colors);
        free(modes);
        free(is_directory);
    }
    
    if (errno != 0) {
        perror("readdir failed");
    }
    
    closedir(dp);
}
