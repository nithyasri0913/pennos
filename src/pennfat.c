#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "FAT/commands.h"
#include "FAT/fat_structs.h"
#include "FAT/fd_table.h"
#include "FAT/k_functions.h"
#include "FAT/s_functions.h"
#include "FAT/shell_commands.h"
#include "parser/parser.h"
// Feel free to modify or not use this document
// this is just the skeleton of what stand alone pennfat
// will probably look like for you

#define MAX_LINE_LENGTH 200
#define PROMPT "pennfat> "

int main(int argc, char* argv[]) {
  // TODO: register signal handlers
  while (1) {
    // TODO: prompt, read command, parse command
    struct parsed_command* cmd;
    char cmd_str[MAX_LINE_LENGTH];

    // Printing Prompt
    write(STDOUT_FILENO, PROMPT, strlen(PROMPT));

    // Read input
    ssize_t num_bytes = read(STDIN_FILENO, cmd_str, MAX_LINE_LENGTH);

    // Check for EOF (Ctrl+D)
    if (num_bytes <= 0) {
      break;
    }

    // Null-terminate the string
    cmd_str[num_bytes] = '\0';

    // Parse the command
    int error_code = parse_command(cmd_str, &cmd);

    // Check if parse succeeded
    if (error_code != 0 || cmd == NULL) {
      continue;
    }

    char** args = cmd->commands[0];

    // Check if args exist
    if (args == NULL || args[0] == NULL) {
      free(cmd);
      continue;
    }

    // Execute commands
    if (strcmp(args[0], "mkfs") == 0) {
      mkfs(args[1], strtol(args[2], NULL, 10), strtol(args[3], NULL, 10));
    } else if (strcmp(args[0], "mount") == 0) {
      mount(args[1]);
    } else if (strcmp(args[0], "unmount") == 0) {
      unmount();
    } else if (strcmp(args[0], "print_fat") == 0) {
      print_fat_table();
    }
    // only if mounted
    else if (strcmp(args[0], "test_open") == 0) {
      if (_current_mount == NULL) {
        s_printf("Error: No filesystem mounted!\n");
      } else {
        s_printf("\nTesting k_open \n");

        // Test 1: Create new file in WRITE mode
        s_printf("\nTest 1: Create new file\n");
        int fd1 = k_open("test1.txt", F_WRITE);
        s_printf("  k_open('test1.txt', F_WRITE) = %d (expect 3)\n", fd1);
        if (fd1 >= 0) {
          s_printf(" File created, FD allocated\n");
          s_printf("  FD info: size=%u, first_block=%u, offset=%ld\n",
                 global_fd_table[fd1].size, global_fd_table[fd1].first_block,
                 global_fd_table[fd1].offset);
        }

        // Test 2: Try to open non-existent file in READ mode
        s_printf("\nTest 2: Open non-existent file in READ mode\n");
        int fd2 = k_open("nonexistent.txt", F_READ);
        s_printf("  k_open('nonexistent.txt', F_READ) = %d (expect %d)\n", fd2,
               FILE_NOT_FOUND);
        if (fd2 == FILE_NOT_FOUND) {
          s_printf("  Correctly rejected\n");
        } else {
          s_printf("  Should have returned FILE_NOT_FOUND\n");
        }

        // Test 3: Try to open same file twice in WRITE mode
        s_printf("\nTest 3: Open same file twice in WRITE mode\n");
        int fd3 = k_open("test1.txt", F_WRITE);
        s_printf("  k_open('test1.txt', F_WRITE) = %d (expect %d)\n", fd3,
               FILE_ALREADY_OPEN);
        if (fd3 == FILE_ALREADY_OPEN) {
          s_printf(" Correctly rejected\n");
        } else {
          s_printf("Should have returned FILE_ALREADY_OPEN\n");
        }

        // Test 4: Open in APPEND mode (should work)
        s_printf("\nTest 4: Create file in APPEND mode\n");
        int fd4 = k_open("test2.txt", F_APPEND);
        s_printf("  k_open('test2.txt', F_APPEND) = %d (expect 4)\n", fd4);
        if (fd4 >= 0) {
          s_printf("File created in APPEND mode\n");
          s_printf("  FD info: offset=%ld (should be at end)\n",
                 global_fd_table[fd4].offset);
        }

        // Test 5: Open same file in READ mode (multiple readers allowed)
        s_printf("\nTest 5: Open existing file in READ mode\n");
        int fd5 = k_open("test1.txt", F_READ);
        s_printf("  k_open('test1.txt', F_READ) = %d (expect 5)\n", fd5);
        if (fd5 >= 0) {
          s_printf("Multiple readers allowed\n");
        }

        s_printf("\nTests Complete \n");
      }
    } else if (strcmp(args[0], "test_write") == 0) {
      s_printf("\nTesting k_write \n");

      int fd = k_open("writefile.txt", F_WRITE);
      s_printf("Opened file, fd=%d\n", fd);

      const char* text = "Hello, World!";
      int written = k_write(fd, text, strlen(text));
      s_printf("Wrote %d bytes (expected %zu)\n", written, strlen(text));

      s_printf("File size now: %u\n", global_fd_table[fd].size);
      s_printf("First block: %u\n", global_fd_table[fd].first_block);

      s_printf("Test Complete \n");
    } else if (strcmp(args[0], "test_read") == 0) {
      s_printf("\n Testing k_read\n");

      // write a file
      int fd_w = k_open("readtest.txt", F_WRITE);
      const char* text = "Hello, World! This is a test.";
      k_write(fd_w, text, strlen(text));
      k_close(fd_w);
      s_printf("Wrote test file\n");

      // reading it
      int fd_r = k_open("readtest.txt", F_READ);
      char buf[100];
      memset(buf, 0, 100);
      int read_bytes = k_read(fd_r, 50, buf);

      s_printf("Read %d bytes\n", read_bytes);
      s_printf("Content: '%s'\n", buf);

      if (strcmp(buf, text) == 0) {
        s_printf("Read matches write!\n");
      } else {
        s_printf("Content mismatch!\n");
      }

      s_printf("Test Complete\n");
    } else if (strcmp(args[0], "test_complete") == 0) {
      s_printf("k_functions TEST\n");

      // Test 1: k_open + k_write + k_close
      s_printf("\n[TEST 1] Create and write file\n");
      int fd1 = k_open("file1.txt", F_WRITE);
      s_printf("  k_open('file1.txt', F_WRITE) = %d\n", fd1);
      const char* data1 = "This is test file 1!";
      int w1 = k_write(fd1, data1, strlen(data1));
      s_printf("  k_write() = %d bytes\n", w1);
      k_close(fd1);
      s_printf("  k_close() - file closed\n");

      // Test 2: k_open (READ) + k_read
      s_printf("\n[TEST 2] Read file back\n");
      int fd2 = k_open("file1.txt", F_READ);
      char buf[100];
      memset(buf, 0, 100);
      int r1 = k_read(fd2, 50, buf);
      s_printf("  k_read() = %d bytes: '%s'\n", r1, buf);
      k_close(fd2);

      // Test 3: k_lseek
      s_printf("\n[TEST 3] lseek operations\n");
      int fd3 = k_open("file1.txt", F_READ);
      k_lseek(fd3, 5, F_SEEK_SET);
      memset(buf, 0, 100);
      k_read(fd3, 4, buf);
      s_printf("  Read from offset 5: '%s' (expect 'is t')\n", buf);
      k_close(fd3);

      // Test 4: k_open (APPEND)
      s_printf("\n[TEST 4] Append to file\n");
      int fd4 = k_open("file1.txt", F_APPEND);
      const char* data2 = " APPENDED!";
      k_write(fd4, data2, strlen(data2));
      s_printf("  Appended '%s'\n", data2);
      k_close(fd4);

      // Verify append
      int fd5 = k_open("file1.txt", F_READ);
      memset(buf, 0, 100);
      k_read(fd5, 50, buf);
      s_printf("  Full content: '%s'\n", buf);
      k_close(fd5);

      // Test 5: k_ls
      s_printf("\n[TEST 5] List files\n");
      k_ls(NULL);

      // Test 6: Multiple files
      s_printf("\n[TEST 6] Create multiple files\n");
      for (int i = 2; i <= 5; i++) {
        char fname[20];
        sprintf(fname, "file%d.txt", i);
        int fd = k_open(fname, F_WRITE);
        char data[50];
        sprintf(data, "Content of file %d", i);
        k_write(fd, data, strlen(data));
        k_close(fd);
        s_printf("  Created %s\n", fname);
      }

      s_printf("\n  All files:\n");
      k_ls(NULL);

      // Test 7: k_unlink
      s_printf("\n[TEST 7] Delete file\n");
      int unlink_result = k_unlink("file3.txt");
      s_printf("  k_unlink('file3.txt') = %d\n", unlink_result);
      s_printf("  Files after deletion:\n");
      k_ls(NULL);

      // Test 8: Try to unlink open file
      s_printf("\n[TEST 8] Try to delete open file\n");
      int fd6 = k_open("file2.txt", F_READ);
      int unlink_fail = k_unlink("file2.txt");
      s_printf("  k_unlink('file2.txt') while open = %d (should fail)\n",
             unlink_fail);
      k_close(fd6);

      // Test 9: Large file (crosses blocks)
      s_printf("\n[TEST 9] Large file test\n");
      int fd7 = k_open("bigfile.txt", F_WRITE);
      char big_data[600];
      memset(big_data, 'X', 600);
      big_data[599] = '\0';
      int w2 = k_write(fd7, big_data, 600);
      s_printf("  Wrote %d bytes to bigfile.txt\n", w2);
      k_close(fd7);

      // Verify large file
      int fd8 = k_open("bigfile.txt", F_READ);
      char verify[700];
      memset(verify, 0, 700);
      int r2 = k_read(fd8, 700, verify);
      s_printf("  Read back %d bytes\n", r2);
      k_close(fd8);

      // Test 10: Overwrite file (F_WRITE truncates)
      s_printf("\n[TEST 10] Overwrite file\n");
      s_printf("  Before overwrite:\n  ");
      k_ls("file1.txt");
      int fd9 = k_open("file1.txt", F_WRITE);
      const char* new_data = "NEW!";
      k_write(fd9, new_data, strlen(new_data));
      k_close(fd9);
      s_printf("  After overwrite:\n  ");
      k_ls("file1.txt");

      s_printf("ALL TESTS COMPLETE!\n");
    } else if (strcmp(args[0], "ls") == 0) {
      cmd_ls(args);
    } else if (strcmp(args[0], "test_touch") == 0) {
      s_printf("\n=== TESTING touch COMMAND ===\n");

      // Test 1: Create new file with touch
      s_printf("\n[TEST 1] Create new file with touch\n");
      char* touch_args1[] = {"touch", "touched1.txt", NULL};
      touch(touch_args1);
      s_printf("  Files after touch:\n");
      k_ls("touched1.txt");

      // Test 2: Touch multiple files
      s_printf("\n[TEST 2] Touch multiple files\n");
      char* touch_args2[] = {"touch", "touched2.txt", NULL};
      touch(touch_args2);
      char* touch_args3[] = {"touch", "touched3.txt", NULL};
      touch(touch_args3);
      s_printf("  All touched files:\n");
      k_ls(NULL);

      // Test 3: Touch existing file (should not modify)
      s_printf("\n[TEST 3] Touch existing file\n");
      int fd_write = k_open("existing.txt", F_WRITE);
      const char* data = "Original content";
      k_write(fd_write, data, strlen(data));
      k_close(fd_write);
      s_printf("  Before touch:\n  ");
      k_ls("existing.txt");
      char* touch_args4[] = {"touch", "existing.txt", NULL};
      touch(touch_args4);
      s_printf("  After touch:\n  ");
      k_ls("existing.txt");

      // Verify content is still there
      int fd_read = k_open("existing.txt", F_READ);
      char buf[50];
      memset(buf, 0, 50);
      int read_bytes = k_read(fd_read, 50, buf);
      s_printf("  Content: '%s' (%d bytes)\n", buf, read_bytes);
      k_close(fd_read);

      s_printf("\n=== touch TESTS COMPLETE ===\n");
    } else if (strcmp(args[0], "test_mv") == 0) {
      s_printf("\n=== TESTING mv COMMAND ===\n");

      // Test 1: Rename a file
      s_printf("\n[TEST 1] Rename a file\n");
      int fd1 = k_open("oldname.txt", F_WRITE);
      const char* content1 = "This file will be renamed";
      k_write(fd1, content1, strlen(content1));
      k_close(fd1);
      s_printf("  Before mv:\n");
      k_ls(NULL);

      char* mv_args1[] = {"mv", "oldname.txt", "newname.txt", NULL};
      mv(mv_args1);
      s_printf("  After mv:\n");
      k_ls(NULL);

      // Verify content is preserved
      int fd_verify = k_open("newname.txt", F_READ);
      char buf[50];
      memset(buf, 0, 50);
      k_read(fd_verify, 50, buf);
      s_printf("  Content of newname.txt: '%s'\n", buf);
      k_close(fd_verify);

      // Test 2: Try to move non-existent file
      s_printf("\n[TEST 2] Try to move non-existent file\n");
      char* mv_args2[] = {"mv", "nonexistent.txt", "destination.txt", NULL};
      mv(mv_args2);

      // Test 3: Try to move to existing file (should fail)
      s_printf("\n[TEST 3] Try to move to existing filename\n");
      int fd2 = k_open("file_a.txt", F_WRITE);
      k_write(fd2, "File A", 6);
      k_close(fd2);
      int fd3 = k_open("file_b.txt", F_WRITE);
      k_write(fd3, "File B", 6);
      k_close(fd3);

      s_printf("  Before mv (should fail):\n");
      k_ls(NULL);
      char* mv_args3[] = {"mv", "file_a.txt", "file_b.txt", NULL};
      mv(mv_args3);
      s_printf("  After mv attempt:\n");
      k_ls(NULL);

      // Test 4: Rename with long filename
      s_printf("\n[TEST 4] Rename with valid filename\n");
      int fd4 = k_open("short.txt", F_WRITE);
      k_write(fd4, "Short name", 10);
      k_close(fd4);
      char* mv_args4[] = {"mv", "short.txt", "much_longer_filename.txt", NULL};
      mv(mv_args4);
      s_printf("  Files after rename:\n");
      k_ls(NULL);

      s_printf("\n=== mv TESTS COMPLETE ===\n");
    } else if (strcmp(args[0], "test_rm") == 0) {
      s_printf("\n=== TESTING rm COMMAND ===\n");

      // Test 1: Remove a file
      s_printf("\n[TEST 1] Remove a file\n");
      int fd1 = k_open("delete_me.txt", F_WRITE);
      const char* content1 = "This file will be deleted";
      k_write(fd1, content1, strlen(content1));
      k_close(fd1);
      s_printf("  Before rm:\n");
      k_ls(NULL);

      char* rm_args1[] = {"rm", "delete_me.txt", NULL};
      rm(rm_args1);
      s_printf("  After rm:\n");
      k_ls(NULL);

      // Test 2: Try to remove non-existent file
      s_printf("\n[TEST 2] Try to remove non-existent file\n");
      char* rm_args2[] = {"rm", "nonexistent.txt", NULL};
      rm(rm_args2);

      // Test 3: Try to remove an open file (should fail)
      s_printf("\n[TEST 3] Try to remove an open file\n");
      int fd2 = k_open("open_file.txt", F_WRITE);
      k_write(fd2, "Content", 7);
      s_printf("  File is open (fd=%d)\n", fd2);
      char* rm_args3[] = {"rm", "open_file.txt", NULL};
      rm(rm_args3);
      s_printf("  Closing file and trying again...\n");
      k_close(fd2);
      rm(rm_args3);
      s_printf("  After successful rm:\n");
      k_ls(NULL);

      // Test 4: Remove multiple files
      s_printf("\n[TEST 4] Create and remove multiple files\n");
      for (int i = 1; i <= 3; i++) {
        char fname[20];
        sprintf(fname, "temp%d.txt", i);
        int fd = k_open(fname, F_WRITE);
        char data[30];
        sprintf(data, "Temp file %d", i);
        k_write(fd, data, strlen(data));
        k_close(fd);
      }
      s_printf("  Created temp files:\n");
      k_ls(NULL);

      for (int i = 1; i <= 3; i++) {
        char fname[20];
        sprintf(fname, "temp%d.txt", i);
        char* rm_args[] = {"rm", fname, NULL};
        rm(rm_args);
        s_printf("  Removed %s\n", fname);
      }
      s_printf("  After removing all temp files:\n");
      k_ls(NULL);

      // Test 5: Remove file and verify it's gone
      s_printf("\n[TEST 5] Verify file cannot be accessed after removal\n");
      int fd3 = k_open("verify_delete.txt", F_WRITE);
      k_write(fd3, "Delete and verify", 17);
      k_close(fd3);
      s_printf("  File created\n");

      char* rm_args4[] = {"rm", "verify_delete.txt", NULL};
      rm(rm_args4);

      int fd4 = k_open("verify_delete.txt", F_READ);
      if (fd4 == FILE_NOT_FOUND) {
        s_printf("  File correctly cannot be opened (FILE_NOT_FOUND)\n");
      } else {
        s_printf("  ERROR: File should not exist! fd=%d\n", fd4);
        if (fd4 >= 0)
          k_close(fd4);
      }

      // Test 6: Remove file opened in read mode
      s_printf("\n[TEST 6] Try to remove file opened in read mode\n");
      int fd5 = k_open("read_open.txt", F_WRITE);
      k_write(fd5, "Read mode test", 14);
      k_close(fd5);

      int fd6 = k_open("read_open.txt", F_READ);
      s_printf("  File opened for reading (fd=%d)\n", fd6);
      char* rm_args5[] = {"rm", "read_open.txt", NULL};
      rm(rm_args5);
      k_close(fd6);
      s_printf("  After closing:\n");
      rm(rm_args5);

      s_printf("\n=== rm TESTS COMPLETE ===\n");
    } else if (strcmp(args[0], "touch") == 0) {
      if (args[1] == NULL) {
        s_printf("touch: missing file operand\n");
      } else {
        touch(args);
      }
    } else if (strcmp(args[0], "mv") == 0) {
      if (args[1] == NULL || args[2] == NULL) {
        s_printf("mv: missing file operand\n");
        s_printf("Usage: mv SOURCE DEST\n");
      } else {
        mv(args);
      }
    } else if (strcmp(args[0], "rm") == 0) {
      if (args[1] == NULL) {
        s_printf("rm: missing file operand\n");
      } else {
        rm(args);
      }
    } else if (strcmp(args[0], "test_cat") == 0) {
      s_printf("\n=== TESTING cat COMMAND ===\n");

      // Test 1: Read and display a single file
      s_printf("\n[TEST 1] Read and display a single file\n");
      int fd1 = k_open("cat_test1.txt", F_WRITE);
      const char* content1 = "Hello from cat test file 1!";
      k_write(fd1, content1, strlen(content1));
      k_close(fd1);
      s_printf("  Created cat_test1.txt with content\n");
      s_printf("  Running: cat cat_test1.txt\n");
      s_printf("  Output:\n");
      char* cat_args1[] = {"cat", "cat_test1.txt", NULL};
      cat(cat_args1);

      // Test 2: Concatenate multiple files
      s_printf("\n[TEST 2] Concatenate multiple files\n");
      int fd2 = k_open("cat_test2.txt", F_WRITE);
      const char* content2 = "This is file 2.";
      k_write(fd2, content2, strlen(content2));
      k_close(fd2);

      int fd3 = k_open("cat_test3.txt", F_WRITE);
      const char* content3 = "And this is file 3!";
      k_write(fd3, content3, strlen(content3));
      k_close(fd3);

      s_printf("  Created cat_test2.txt and cat_test3.txt\n");
      s_printf("  Running: cat cat_test1.txt cat_test2.txt cat_test3.txt\n");
      s_printf("  Output:\n");
      char* cat_args2[] = {"cat", "cat_test1.txt", "cat_test2.txt",
                           "cat_test3.txt", NULL};
      cat(cat_args2);

      // Test 3: Write to file with -w flag (overwrite)
      s_printf("\n[TEST 3] Write to file with -w flag (overwrite)\n");
      s_printf("  Running: cat cat_test1.txt cat_test2.txt -w output1.txt\n");
      char* cat_args3[] = {"cat", "cat_test1.txt", "cat_test2.txt",
                           "-w",  "output1.txt",   NULL};
      cat(cat_args3);
      s_printf("  Reading output1.txt:\n");
      int fd_out1 = k_open("output1.txt", F_READ);
      char buf1[200];
      memset(buf1, 0, 200);
      int bytes1 = k_read(fd_out1, 200, buf1);
      s_printf("  Content (%d bytes): '%s'\n", bytes1, buf1);
      k_close(fd_out1);

      // Test 4: Append to file with -a flag
      s_printf("\n[TEST 4] Append to file with -a flag\n");
      s_printf("  Running: cat cat_test3.txt -a output1.txt\n");
      char* cat_args4[] = {"cat", "cat_test3.txt", "-a", "output1.txt", NULL};
      cat(cat_args4);
      s_printf("  Reading output1.txt after append:\n");
      int fd_out2 = k_open("output1.txt", F_READ);
      char buf2[200];
      memset(buf2, 0, 200);
      int bytes2 = k_read(fd_out2, 200, buf2);
      s_printf("  Content (%d bytes): '%s'\n", bytes2, buf2);
      k_close(fd_out2);

      // Test 5: cat -w to create new file from terminal input (manual test)
      s_printf("\n[TEST 5] cat -w OUTPUT_FILE (terminal input)\n");
      s_printf("  This mode reads from terminal. Skipping automated test.\n");
      s_printf("  Manual test: Run 'cat -w newfile.txt' and type content.\n");

      // Test 6: cat -a to append from terminal input (manual test)
      s_printf("\n[TEST 6] cat -a OUTPUT_FILE (terminal input)\n");
      s_printf("  This mode reads from terminal. Skipping automated test.\n");
      s_printf(
          "  Manual test: Run 'cat -a existingfile.txt' and type content.\n");

      // Test 7: Try to cat non-existent file
      s_printf("\n[TEST 7] Try to cat non-existent file\n");
      s_printf("  Running: cat nonexistent.txt\n");
      char* cat_args5[] = {"cat", "nonexistent.txt", NULL};
      cat(cat_args5);

      // Test 8: cat multiple files with some non-existent (should continue)
      s_printf("\n[TEST 8] cat with some non-existent files\n");
      s_printf(
          "  Running: cat cat_test1.txt missing.txt cat_test2.txt (should "
          "continue)\n");
      char* cat_args6[] = {"cat", "cat_test1.txt", "missing.txt",
                           "cat_test2.txt", NULL};
      cat(cat_args6);

      // Test 9: Overwrite existing file with -w
      s_printf("\n[TEST 9] Overwrite existing file with -w\n");
      int fd_old = k_open("overwrite_me.txt", F_WRITE);
      const char* old_content = "Old content that will be overwritten.";
      k_write(fd_old, old_content, strlen(old_content));
      k_close(fd_old);
      s_printf("  Created overwrite_me.txt with old content\n");
      s_printf("  Before overwrite: ");
      k_ls("overwrite_me.txt");

      s_printf("  Running: cat cat_test1.txt -w overwrite_me.txt\n");
      char* cat_args7[] = {"cat", "cat_test1.txt", "-w", "overwrite_me.txt",
                           NULL};
      cat(cat_args7);

      s_printf("  After overwrite: ");
      k_ls("overwrite_me.txt");
      int fd_verify = k_open("overwrite_me.txt", F_READ);
      char buf_verify[200];
      memset(buf_verify, 0, 200);
      int bytes_verify = k_read(fd_verify, 200, buf_verify);
      s_printf("  New content (%d bytes): '%s'\n", bytes_verify, buf_verify);
      k_close(fd_verify);

      // Test 10: Large file concatenation
      s_printf("\n[TEST 10] Large file concatenation\n");
      int fd_large1 = k_open("large1.txt", F_WRITE);
      char large_data1[300];
      memset(large_data1, 'A', 299);
      large_data1[299] = '\0';
      k_write(fd_large1, large_data1, 299);
      k_close(fd_large1);

      int fd_large2 = k_open("large2.txt", F_WRITE);
      char large_data2[300];
      memset(large_data2, 'B', 299);
      large_data2[299] = '\0';
      k_write(fd_large2, large_data2, 299);
      k_close(fd_large2);

      s_printf("  Created large1.txt (299 A's) and large2.txt (299 B's)\n");
      s_printf("  Running: cat large1.txt large2.txt -w combined.txt\n");
      char* cat_args8[] = {"cat", "large1.txt",   "large2.txt",
                           "-w",  "combined.txt", NULL};
      cat(cat_args8);

      s_printf("  Verifying combined.txt size: ");
      k_ls("combined.txt");
      int fd_combined = k_open("combined.txt", F_READ);
      char large_verify[700];
      memset(large_verify, 0, 700);
      int bytes_large = k_read(fd_combined, 700, large_verify);
      s_printf("  Read %d bytes from combined.txt\n", bytes_large);
      if (bytes_large == 598) {
        s_printf("  Size correct!\n");
      } else {
        s_printf("  ERROR: Expected 598 bytes, got %d\n", bytes_large);
      }
      k_close(fd_combined);

      // Test 11: Empty file
      s_printf("\n[TEST 11] cat empty file\n");
      int fd_empty = k_open("empty.txt", F_WRITE);
      k_close(fd_empty);
      s_printf("  Created empty.txt\n");
      s_printf("  Running: cat empty.txt\n");
      s_printf("  Output:\n");
      char* cat_args9[] = {"cat", "empty.txt", NULL};
      cat(cat_args9);
      s_printf("  (should show nothing)\n");

      // Test 12: Verify all created files
      s_printf("\n[TEST 12] List all files created during tests\n");
      k_ls(NULL);

      s_printf("\n=== cat TESTS COMPLETE ===\n");
    } else if (strcmp(args[0], "test_cp") == 0) {
      s_printf("\n=== TESTING cp COMMAND ===\n");

      // Test 1: Copy within filesystem
      s_printf("\n[TEST 1] Copy within filesystem\n");
      int fd1 = k_open("source1.txt", F_WRITE);
      const char* content1 = "This is the source file content.";
      k_write(fd1, content1, strlen(content1));
      k_close(fd1);
      s_printf("  Created source1.txt\n");

      s_printf("  Running: cp source1.txt dest1.txt\n");
      char* cp_args1[] = {"cp", "source1.txt", "dest1.txt", NULL};
      cp(cp_args1);

      s_printf("  Verifying dest1.txt:\n");
      int fd_verify1 = k_open("dest1.txt", F_READ);
      char buf1[100];
      memset(buf1, 0, 100);
      int bytes1 = k_read(fd_verify1, 100, buf1);
      s_printf("  Content (%d bytes): '%s'\n", bytes1, buf1);
      k_close(fd_verify1);

      // Test 2: Try to copy to existing file (should fail)
      s_printf("\n[TEST 2] Try to copy to existing file (should fail)\n");
      s_printf("  Running: cp source1.txt dest1.txt\n");
      char* cp_args2[] = {"cp", "source1.txt", "dest1.txt", NULL};
      cp(cp_args2);

      // Test 3: Copy non-existent file (should fail)
      s_printf("\n[TEST 3] Try to copy non-existent file\n");
      s_printf("  Running: cp nonexistent.txt newfile.txt\n");
      char* cp_args3[] = {"cp", "nonexistent.txt", "newfile.txt", NULL};
      cp(cp_args3);

      // Test 4: Copy from host to filesystem (requires manual file creation)
      s_printf("\n[TEST 4] Copy from host to filesystem (cp -h SOURCE DEST)\n");
      s_printf("  Creating a test file on host filesystem...\n");
      FILE* host_file = fopen("host_test.txt", "w");
      if (host_file != NULL) {
        fprintf(host_file, "This file is from the host OS!");
        fclose(host_file);
        s_printf("  Created host_test.txt on host OS\n");

        s_printf("  Running: cp -h host_test.txt fs_from_host.txt\n");
        char* cp_args4[] = {"cp", "-h", "host_test.txt", "fs_from_host.txt",
                            NULL};
        cp(cp_args4);

        s_printf("  Verifying fs_from_host.txt in filesystem:\n");
        int fd_verify2 = k_open("fs_from_host.txt", F_READ);
        char buf2[100];
        memset(buf2, 0, 100);
        int bytes2 = k_read(fd_verify2, 100, buf2);
        s_printf("  Content (%d bytes): '%s'\n", bytes2, buf2);
        k_close(fd_verify2);
      } else {
        s_printf("  Could not create host test file\n");
      }

      // Test 5: Copy from filesystem to host
      s_printf("\n[TEST 5] Copy from filesystem to host (cp SOURCE -h DEST)\n");
      int fd2 = k_open("fs_source.txt", F_WRITE);
      const char* content2 = "This file will be copied to host OS.";
      k_write(fd2, content2, strlen(content2));
      k_close(fd2);
      s_printf("  Created fs_source.txt in filesystem\n");

      s_printf("  Running: cp fs_source.txt -h host_dest.txt\n");
      char* cp_args5[] = {"cp", "fs_source.txt", "-h", "host_dest.txt", NULL};
      cp(cp_args5);

      s_printf("  Verifying host_dest.txt on host OS:\n");
      FILE* host_verify = fopen("host_dest.txt", "r");
      if (host_verify != NULL) {
        char host_buf[100];
        memset(host_buf, 0, 100);
        size_t host_bytes = fread(host_buf, 1, 100, host_verify);
        s_printf("  Content (%zu bytes): '%s'\n", host_bytes, host_buf);
        fclose(host_verify);
      } else {
        s_printf("  Could not open host_dest.txt\n");
      }

      // Test 6: Large file copy
      s_printf("\n[TEST 6] Large file copy\n");
      int fd_large = k_open("large_source.txt", F_WRITE);
      char large_data[800];
      memset(large_data, 'X', 799);
      large_data[799] = '\0';
      k_write(fd_large, large_data, 799);
      k_close(fd_large);
      s_printf("  Created large_source.txt (799 bytes)\n");

      s_printf("  Running: cp large_source.txt large_dest.txt\n");
      char* cp_args6[] = {"cp", "large_source.txt", "large_dest.txt", NULL};
      cp(cp_args6);

      s_printf("  Verifying large_dest.txt size: ");
      k_ls("large_dest.txt");

      // Test 7: Copy empty file
      s_printf("\n[TEST 7] Copy empty file\n");
      int fd_empty = k_open("empty_source.txt", F_WRITE);
      k_close(fd_empty);
      s_printf("  Created empty_source.txt\n");

      s_printf("  Running: cp empty_source.txt empty_dest.txt\n");
      char* cp_args7[] = {"cp", "empty_source.txt", "empty_dest.txt", NULL};
      cp(cp_args7);

      s_printf("  Verifying empty_dest.txt: ");
      k_ls("empty_dest.txt");

      // Test 8: List all files
      s_printf("\n[TEST 8] List all files created during tests\n");
      k_ls(NULL);

      s_printf("\n=== cp TESTS COMPLETE ===\n");
    } else if (strcmp(args[0], "cat") == 0) {
      cat(args);
    } else if (strcmp(args[0], "cp") == 0) {
      if (args[1] == NULL || args[2] == NULL) {
        s_printf("cp: missing file operand\n");
        s_printf("Usage: cp [-h] SOURCE DEST  OR  cp SOURCE -h DEST\n");
      } else {
        cp(args);
      }
    } else if (strcmp(args[0], "chmod") == 0) {
      cmd_chmod(args);
    } else {
      s_fprintf_stderr( "pennfat: command not found: %s\n", args[0]);
    }
    free(cmd);
    // else if (strcmp(args[0], "touch") == 0)
    // {
    //   TODO: Call your implemented touch() function
    // }
    // else if (strcmp(args[0], "cat") == 0)
    // {
    //   TODO: Call your implemented cat() function
    // }
    // else if (strcmp(args[0], "chmod") == 0)
    // {
    //   TODO: Call your implemented chmod() function
    // }
    // ...
    //
    // else
    // {
    //   s_fprintf_stderr( "pennfat: command not found: %s\n", args[0]);
    // }
  }
  return EXIT_SUCCESS;
}