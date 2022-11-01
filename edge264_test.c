#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "edge264.h"

#define RESET  "\033[0m"
#define RED    "\033[31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define BLUE   "\033[34m"
#define PURPLE "\033[35m"


int main(int argc, char *argv[])
{
	// read command-line options
	int print_unsupported = 0;
	int print_passed = 0;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: %s [-u] [-p]\n"
			       "-u\tprint names of unsupported files"
			       "-p\tprint names of passed files", argv[0]);
			return 0;
		} else if (strcmp(argv[i], "-u") == 0) {
			print_unsupported = 1;
		} else if (strcmp(argv[i], "-p") == 0) {
			print_passed = 1;
		}
	}
	
	// fill the stack now
	Edge264_stream e = {};
	struct dirent *entry;
	struct stat stC, stD;
	int counts[6] = {};
	
	// parse all clips in the conformance directory
	setbuf(stdout, NULL);
	if (chdir("conformance") < 0) {
		perror("cannot open \"conformance\" directory");
		return 0;
	}
	DIR *dir = opendir(".");
	assert(dir!=NULL);
	printf("0 " GREEN "PASS" RESET ", 0 " YELLOW "UNSUPPORTED" RESET ", 0 " RED "FAIL" RESET "\n");
	while ((entry = readdir(dir))) {
		char *ext = strrchr(entry->d_name, '.');
		if (*(int *)ext != *(int *)".264")
			continue;
		
		// open the clip file and the corresponding yuv file
		int clip = open(entry->d_name, O_RDONLY);
		memcpy(ext, ".yuv", 4);
		int yuv = open(entry->d_name, O_RDONLY);
		*ext = 0;
		if (clip < 0 || yuv < 0) {
			fprintf(stderr, "open(%s) failed: ", entry->d_name);
			perror(NULL);
			return 0;
		}
		
		// memory-map the two files
		fstat(clip, &stC);
		uint8_t *cpb = mmap(NULL, stC.st_size, PROT_READ, MAP_SHARED, clip, 0);
		fstat(yuv, &stD);
		uint8_t *dpb = mmap(NULL, stD.st_size, PROT_READ, MAP_SHARED, yuv, 0);
		assert(cpb!=MAP_FAILED&&dpb!=MAP_FAILED);
		e.CPB = cpb + 3 + (cpb[2] == 0);
		e.end = cpb + stC.st_size;
		const uint8_t *cmp = dpb;
		
		// decode the entire file and FAIL on any error
		int res;
		do {
			res = Edge264_decode_NAL(&e);
			if (!Edge264_get_frame(&e, res == -2)) {
				int diff = 0;
				for (int y = 0; y < e.height_Y; y++, cmp += e.width_Y << e.pixel_depth_Y)
					diff |= memcmp(e.samples_Y + y * e.stride_Y, cmp, e.width_Y << e.pixel_depth_Y);
				for (int y = 0; y < e.height_C; y++, cmp += e.width_C << e.pixel_depth_C)
					diff |= memcmp(e.samples_Cb + y * e.stride_C, cmp, e.width_C << e.pixel_depth_C);
				for (int y = 0; y < e.height_C; y++, cmp += e.width_C << e.pixel_depth_C)
					diff |= memcmp(e.samples_Cr + y * e.stride_C, cmp, e.width_C << e.pixel_depth_C);
				if (diff)
					res = 2;
			} else if (res == -2) {
				break;
			}
		} while (res <= 0);
		if (res == -2 && cmp != dpb + stD.st_size)
			res = 2;
		Edge264_clear(&e);
		counts[2 + res]++;
		printf("\033[A\033[K"); // move cursor up and clear line
		if (res == -2 && print_passed) {
			printf("%s: " GREEN "PASS" RESET "\n", entry->d_name);
		} else if (res == 1 && print_unsupported) {
			printf("%s: " YELLOW "UNSUPPORTED" RESET "\n", entry->d_name);
		} else if (res >= 2) {
			printf("%s: %s" RESET "\n", entry->d_name, res == 2 ? RED "FAIL" : BLUE "FLAGGED");
		}
		printf("%d " GREEN "PASS" RESET ", %d " YELLOW "UNSUPPORTED" RESET ", %d " RED "FAIL" RESET,
			counts[0], counts[3], counts[4]);
		if (counts[5] > 0)
			printf(", %d " BLUE "FLAGGED" RESET, counts[5]);
		putchar('\n');
		
		// close everything
		munmap(cpb, stC.st_size);
		munmap(dpb, stD.st_size);
		close(clip);
		close(yuv);
	}
	closedir(dir);
	return counts[4];
}
