#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "userprog/syscall.h"
#include "devices/block.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
  flush_cache ();
  clear_cache ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  struct dir *last = get_last_directory (name);
  if (last == false) {
    return false;
  }
  char *filename = malloc (NAME_MAX + 1);
  char *iterpath = malloc (strlen (name) + 1);
  char *path = iterpath;

  strlcpy (path, name, strlen (name) + 1);
  while (get_next_part (filename, &path) == 1);
  block_sector_t new_file_sector = 0;
  free (iterpath);

  if (free_map_allocate (1, &new_file_sector) == false)
    {
      free (filename);
      return false;
    }

  if (inode_create (new_file_sector, initial_size, false) == false)
    {
      dir_close (last);
      free (filename);
      return false;
    }
  else
    {
      if (dir_add (last, filename, new_file_sector, false) == false)
        {
          dir_close (last);
          free_map_release (new_file_sector, 1);
          free (filename);
          return false;
        }
      else
        {
          dir_close (last);
          free (filename);
          return true;
        }
    }
}

const char*
fileandpath(const char *name)
{
  char *filename = malloc (NAME_MAX + 1);
  char *iterpath = malloc (strlen (name) + 1);
  char *path = iterpath;
  strlcpy (path, name, strlen (name) + 1);
  while (get_next_part (filename, &path) == 1);
  free (iterpath);
  return filename;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *last = get_last_directory (name);
  if (last == false) {
    return false;
  }
  char *filename = fileandpath(name);
  struct inode *i;
  dir_lookup (last, filename, &i);
  free (filename);
  dir_close (last);
  return file_open (i);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  struct dir *last = get_last_directory (name);
  if (last == false) {
    return false;
  }
  if (strcmp (name, "/") == false) {
    return false;
  }
  char *filename = fileandpath(name);
  bool result = dir_remove (last, filename);
  dir_close (last);
  free (filename);
  return result;
}

int
num_disk_writes()
{
  return get_write_cnt (fs_device);
}

int
num_disk_reads()
{
  return get_read_cnt (fs_device);
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
