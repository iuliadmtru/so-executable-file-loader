/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "exec_parser.h"

static so_exec_t *exec;
static struct sigaction old_handler;
static int exec_fd;

// checks is a page is mapped in the segment, map it if it is not
bool is_mapped(int page_index, so_seg_t *segment)
{
    // allocate memory for the data segment if necessary, and set it to 0
    int pagesz = getpagesize();
    if (!segment->data) {
        segment->data = calloc((segment->mem_size - segment->vaddr) / pagesz + 1,
                               sizeof(char));
    }
    // check if the page is mapped
    if (((int *)segment->data)[page_index] == 1) {
        return true;
    }
    // map it
    return false;
}

so_seg_t *find_segment_with_fault(void *fault_addr)
{
    for (int i = 0; i < exec->segments_no; i++) {
        // get the difference between the address where the signal was
        // generated and the virtual address of the i-th segment
        size_t diff = (char *)fault_addr - (char *)exec->segments[i].vaddr;
        // the signal-generating address is inside the segment if it is larger
        // than or equal to the segment start address and if the difference
        // between the two addresses is smaller than the size of a segment
        if (diff >= 0 && diff < exec->segments[i].mem_size) {
            return &exec->segments[i];
        }
    }
    return NULL;
}

// copies the page contents to the segment
void copy_page_to_segment(void *page_addr, so_seg_t *segment, int page_index)
{
    int pagesz = getpagesize();
    int offset = pagesz * page_index;
    if (segment->file_size >= offset) {
        // move the cursor to the page address
        lseek(exec_fd, segment->offset + offset, SEEK_SET);
        int min = offset + pagesz <= segment->file_size ? pagesz
                                  : segment->file_size - offset;
        read(exec_fd, (void *)segment->vaddr + offset, min);
    }
}

static void segv_handler(int signum, siginfo_t *info, void *context)
{
    int pagesz = getpagesize();

	// find the segment that generated the signal
    so_seg_t *segment = find_segment_with_fault(info->si_addr);
    // default handler if the segment was not found
    if (!segment) {
       old_handler.sa_sigaction(signum, info, context);
    }

    // get the page index
    int page_index = ((char *)info->si_addr - (char *)segment->vaddr) / pagesz;
    // default handler if the page is already mapped
    if (is_mapped(page_index, segment)) {
       old_handler.sa_sigaction(signum, info, context);
    }

    // get the new page address
    void *page_addr = (void *)segment->vaddr + page_index * pagesz;
    // map the page in virtual memory with flags:
    //      MAP_ANON - memory is not associated with any specific file
    //      MAP_FIXED - place mapping at the specified address
    //      MAP_SHARED - share modifications (shared library)
    void *new_page = mmap(page_addr, pagesz, PERM_R | PERM_W,
                          MAP_ANON | MAP_FIXED | MAP_SHARED, -1, 0);
    ((int *)segment->data)[page_index] = 1;
    
    // copy the page contents to the segment
    copy_page_to_segment(new_page, segment, page_index);
    // protect the page according to the segment permissions
    mprotect(new_page, pagesz, segment->perm);
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

    // // set all segment data pointers to NULL
    // printf("before disaster\n");
    // for (int i = 0; i < exec->segments_no; i++) {
    //     exec->segments[i].data = NULL;
    // }
    // printf("after disaster\n");

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, &old_handler);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

    exec_fd = open(path, O_RDONLY);
	so_start_exec(exec, argv);

	return -1;
}
