#ifndef FS_H
#define FS_H

#include "param.h"
#include "types.h"

#define ROOTINO 1

struct superblock {
  uint size;         // Total size (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes
  uint nlog;         // Number of log blocks
  uint logstart;     // First log block
  uint inodestart;   // First inode block
  uint bmapstart;    // First free map block
  uint checksum;     // Fletcher-32 Checksum integritas metadata
};

#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(uint))
#define NDINDIRECT (NINDIRECT * NINDIRECT)
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT)

// Struktur data inti inode
struct dinode_core {
  short type;           
  short major;          
  short minor;          
  short nlink;          
  uint size;            
  uint addrs[NDIRECT+2];
  uint atime;           // Access time
  uint mtime;           // Modified time
  uint ctime;           // Creation time
};

// Union memastikan secara matematis ukuran struct adalah 128 byte
struct dinode {
  union {
    struct dinode_core data;
    uchar pad[128];
  };
};

// --- TAMBAHKAN BAGIAN INI UNTUK MEMPERBAIKI ERROR ---
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};
// ----------------------------------------------------

#define IPB           (BSIZE / sizeof(struct dinode))
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)
#define BPB           (BSIZE * 8)
#define BMAPBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

#endif