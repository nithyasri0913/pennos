#include "commands.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "fat_structs.h"
#include "fd_table.h"
#include "s_functions.h"
uint16_t* _fat = NULL;
int _block_size = 0;
int _fat_size = 0;
FILE* _current_mount = NULL;
int _fs_fd = -1;

// number of blocks allocated to FAT block size
// 0=256, 1=512, 2=1024, 3=2048, or 4=4096
const int BLOCK_SIZES[] = {256, 512, 1024, 2048, 4096};

// mkfs - creates the filesystem file on disk
// mkfs creates. a binary file
// that contains  FAT region (the FAT array) and Data region (starting with root
// dir)

void mkfs(const char* fs_name, int blocks_in_fat, int block_size_config) {
  if (_current_mount != NULL) {
    s_printf("unexpected command\n");
    return;
  }

  if (blocks_in_fat < 1 || blocks_in_fat > 32) {
    s_printf("Error: blocks_in_fat must be 1-32\n");
    return;
  }
  if (block_size_config < 0 || block_size_config > 4) {
    s_printf("Error: block_size_config must be 0-4\n");
    return;
  }

  // calculate block_size
  int block_size = BLOCK_SIZES[block_size_config];

  // calculate FAT size
  int fat_size_bytes = block_size * blocks_in_fat;
  int num_fat_entries = fat_size_bytes / 2;

  // allocate FAT array
  uint16_t* fat = calloc(num_fat_entries, sizeof(uint16_t));
  if (fat == NULL) {
    s_printf("Error: failed to allocate FAT\n");
    return;
  }

  // FAT[0] is metadata - storing LSB and MSB in metadata
  // LSB = block_size_config (8 bits) and MSB = blocks_in_fat (8 bits)
  fat[0] = (blocks_in_fat << 8) | block_size_config;

  // FAT[1] is root dir
  fat[1] = 0xFFFF;

  // open file in write binary mode
  FILE* fs = fopen(fs_name, "wb");

  if (fs == NULL) {
    s_printf("Error creating the file system!\n");
    free(fat);
    return;
  }

  // write FAT to disk
  size_t written = fwrite(fat, sizeof(uint16_t), num_fat_entries, fs);
  if (written != num_fat_entries) {
    s_printf("Error writing FAT\n");
    free(fat);
    fclose(fs);
    return;
  }

  // create empty root dir
  directory_entry_t* root_dir =
      calloc(block_size / sizeof(directory_entry_t), sizeof(directory_entry_t));
  if (root_dir == NULL) {
    s_printf("Error allocating root directory\n");
    free(fat);
    fclose(fs);
    return;
  }

  root_dir[0].name[0] = 0;

  // write root cir to disk
  written = fwrite(root_dir, 1, block_size, fs);
  if (written != block_size) {
    s_printf("Error writing root directory\n");
    free(fat);
    free(root_dir);
    fclose(fs);
    return;
  }

  long total_size;
  if (blocks_in_fat == 1 && block_size_config == 0) {
    total_size = 32768L;
  } else if (blocks_in_fat == 32 && block_size_config == 4) {
    total_size = 268558336L;
  } else {
    long num_data_blocks = num_fat_entries - 1;
    total_size = (long)fat_size_bytes + num_data_blocks * (long)block_size;
  }

  fseek(fs, total_size - 1, SEEK_SET);
  fputc(0, fs);

  fclose(fs);
  free(fat);
  free(root_dir);

  s_printf("File system created: %s\n", fs_name);
}

int mount(const char* fs_name) {
  s_printf("Mounting file system: %s\n", fs_name);

  if (_current_mount != NULL) {
    s_printf("Error mounting the file system(already mounted)\n");
    return 1;
  }
  // open file in read binary mode
  FILE* fs = fopen(fs_name, "rb+");
  if (fs == NULL) {
    s_printf("Error reading the file system!\n");
    return -1;
  }
  uint16_t fat_zero;
  fread(&fat_zero, sizeof(fat_zero), 1, fs);

  // MSB - shift right by 8 then mask to get bottom 8 bits
  uint8_t blocks_in_fat = (fat_zero >> 8) & 0xFF;
  // just mask to get bottom 8 bits
  uint8_t block_size_config = fat_zero & 0xFF;

  int block_size = BLOCK_SIZES[block_size_config];

  int fat_size = block_size * blocks_in_fat;
  // Do not mmap the entire PennFAT file system.
  // Remember, with FAT only the FAT is in memory. Everything else is on disk.
  int fs_fd = fileno(fs);
  // mpa file to program memeory - automaticaaly syncs to disk
  uint16_t* fat =
      mmap(NULL, fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);

  if (fat == MAP_FAILED) {
    s_printf("Error mapping FAT into memory!\n");
    fclose(fs);
    return -1;
  }

  // In mount(), set them:
  _fat = fat;
  _block_size = block_size;
  _fat_size = fat_size;
  _current_mount = fs;
  _fs_fd = fs_fd;

  init_global_fd_table();
  s_printf("Successfully mounted the file system: %s\n", fs_name);
  return 0;
}

int unmount() {
  // Check if mounted
  if (_current_mount == NULL) {
    s_printf("unexpected command\n");
    return 1;
  }

  // unmap FAT from memory
  int result = munmap(_fat, _fat_size);
  if (result != 0) {
    perror("Error unmounting FAT");
    return 1;
  }

  if (fclose(_current_mount) != 0) {
    perror("Error closing file");
    return 1;
  }

  // reset
  _fat = NULL;
  _block_size = 0;
  _fat_size = 0;
  _current_mount = NULL;

  s_printf("File System unmounted.\n");
  return 0;
}
