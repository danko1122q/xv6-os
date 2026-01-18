#ifndef PARAM_H
#define PARAM_H

#define NPROC        64        // Maximum number of processes
#define KSTACKSIZE   4096      // Size of per-process kernel stack
#define NCPU         8         // Maximum number of CPUs
#define NOFILE       64        // Open files per process (increased for game assets)
#define NFILE        100       // Open files per system
#define NINODE       100       // Maximum number of active i-nodes (increased for icons/WADs)
#define NDEV         10        // Maximum major device number
#define ROOTDEV      1         // Device number of file system root disk
#define MAXARG       32        // Max exec arguments
#define MAXOPBLOCKS  10        // Max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS * 3) // Max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS * 3) // Size of disk block cache

// File System Configuration for 500 MB Disk
// Calculation: (500 * 1024 * 1024) / 2048 (BSIZE) = 256,000 blocks
#define FSSIZE       256000    
#define BSIZE        2048      // Block size in bytes

#endif