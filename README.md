Copyright 2022 Iulia Dumitru 321CA

# ELF Executable file loader

## Assignment steps

1. Find the segment that generated the page fault. If no segment was found
use the default handler.
2. Get the index of the page that generated the fault inside the segment. If
the page is not mapped inside the `data` field of the segment use the default
handler.
3. Get the address of the page that needs to be mapped.
4. Map the page using `mmap` and mark it as mapped in the `data` field of the
segment.
5. Copy the page contents to the segment.
6. Protect the page according to the segment permissions.

## Notes

- The `data` fields of all segments must be set to `NULL` after allocating each
segment. This is necessary for checking whether a page is already mapped and is
done inside skel-lin/loader/exec_parser.c.

- The executable structure, executable file descriptor and default handler must
be declared globally.


## Further improvement

### Self improvement

- Start the homework earlier.
- Do more research. There are a lot of interesting things to read about related
to assignments.

### SO improvement

- Nothing to add for this assignment.
