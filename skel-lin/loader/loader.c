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
#include <fcntl.h>

#include "exec_parser.h"

#define SIGSEGV_EXIT_CODE 139

typedef struct so_page {
    void *page_addr;
    struct so_page *next;
} so_page_t;

typedef struct so_page_list_t {
    so_page_t *mapped_pages;
} so_page_list_t;

static so_exec_t *exec;
static so_page_list_t *page_list;
static int exec_fd;

// initializes loader struct as a list of pages
void build_page_list(so_page_list_t *page_list)
{
    page_list = malloc(sizeof(so_page_list_t));
    page_list->mapped_pages = NULL;
}

// adds a page to the loader
void add_page(void *page_addr, so_page_list_t *page_list)
{
    so_page_t *new = malloc(sizeof(so_page_t));
    new->page_addr = page_addr;
    new->next = page_list->mapped_pages;
    page_list->mapped_pages = new;
}

// checks is a page is mapped in the loader
bool is_mapped(void *page_addr, so_page_list_t *page_list)
{
    so_page_t *mapped = page_list->mapped_pages;
    while (mapped) {
        if ((page_addr - mapped->page_addr < getpagesize())
            && (page_addr > mapped->page_addr)) {
            return true;
        }
    }
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
void copy_page_to_segment(void *page_addr, so_seg_t *segment, size_t offset)
{
    int pagesz = getpagesize();
    // move the cursor to the page address
    lseek(exec_fd, segment->offset + offset, SEEK_SET);
    // read either a page size of bytes, or up to the end of the segment into
    // a buffer
    void *buffer = malloc(pagesz);
    int min = pagesz < segment->file_size ? pagesz
                                          : segment->file_size - offset;
    read(exec_fd, buffer, min);
    // copy the contents of the buffer at the correct address
    memcpy((void *)(segment->vaddr + offset), buffer, min);

    free(buffer);
}

static void segv_handler(int signum, siginfo_t *info, void *context)
{
    // default handler if the page is already mapped
    if (is_mapped(info->si_addr, page_list)) {
        exit(SIGSEGV_EXIT_CODE);
    }

	// find the segment that generated the signal
    so_seg_t *segment = find_segment_with_fault(info->si_addr);
    // default handler if the segment was not found
    if (!segment) {
        exit(SIGSEGV_EXIT_CODE);
    }

    // a new page must be mapped at the correct address
    //    | fault_addr - |
    //    | segment_addr |
    //     --------------
    //    |   |   |   |   
    //    |p1 |p2 |p3 |
    //    |   |   |   |
    //     --------------
    //    |  offset   |
    //     -----------
    // get the offset
    size_t offset = (char *)info->si_addr - (char *)segment->vaddr;
    offset -= offset % getpagesize();
    // get the new page address
    void *page_addr = (void *)segment->vaddr + offset;
    // map the page with flags:
    //      MAP_ANON - memory is not associated with any specific file
    //      MAP_FIXED - place mapping at the specified address
    //      MAP_SHARED - share modifications (shared library)
    void *new_page_addr = mmap(page_addr, getpagesize(), PERM_R | PERM_W,
                          MAP_ANON | MAP_FIXED | MAP_SHARED, -1, 0);
    
    // add the page to the loader
    add_page(new_page_addr, page_list);
    // protect the page according to the segment permissions
    mprotect(new_page_addr, getpagesize(), segment->perm);
    // copy the page contents to the segment
    copy_page_to_segment(new_page_addr, segment, offset);
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

    build_page_list(page_list);
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
    exec_fd = open(path, O_RDONLY);
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	so_start_exec(exec, argv);

	return -1;
}
