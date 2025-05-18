#include "./starter/png_util/lab_png.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

int is_directory(const char *path) {
    struct stat statbuf;
    
    if (stat(path, &statbuf) != 0) {
        perror("stat");
        return 0;
    }

    return S_ISDIR(statbuf.st_mode);
}

int findpng(const char *path, int *png_count) {
    DIR *p_dir;
    struct dirent *p_dirent;
    char full_path[512];

    if ((p_dir = opendir(path)) == NULL) {
        perror("opendir");
        exit(2);
    }

    while ((p_dirent = readdir(p_dir)) != NULL) {
        // prevents segfaults with relative file paths
        if (strcmp(p_dirent->d_name, ".") == 0 || strcmp(p_dirent->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, p_dirent->d_name);

        if (is_directory(full_path)) {
            findpng(full_path, png_count);
        } else {
            FILE *file = fopen(full_path, "rb");

            if (file == NULL) {
                perror("Error opening file");
                return 1;
            }

            U8 *buffer = malloc(8 * sizeof(U8)); 
            if (buffer == NULL) {
                perror("Error allocating memory");
                fclose(file);
                return 1;
            }

            fread(buffer, 1, 8, file);

            if (is_png(buffer, 8)) {
                printf("%s\n", full_path);
                *png_count += 1;
            }

            free(buffer);
            fclose(file);
        }
    }

    if ( closedir(p_dir) != 0 ) {
        perror("closedir");
        exit(3);
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory name>\n", argv[0]);
        return 1;
    }

    int png_count_result = 0;

    findpng(argv[1], &png_count_result);
    
    if (png_count_result == 0) {
        printf("findpng: No PNG file found\n");
    }

    return 0;
}