#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "linuxmodule.hh"
#include "string.hh"

static proc_dir_entry *all_pde = 0;
static proc_dir_entry *free_pde = 0;

/*
 * Register a proc_dir_entry with Linux
 */

int
click_register_pde(proc_dir_entry *parent, proc_dir_entry *child)
{
  return proc_register(parent, child);
}

/*
 * Unregister a proc_dir_entry, and all its subdirectories
 */

int
click_unregister_pde(proc_dir_entry *child)
{
  while (child->subdir)
    click_unregister_pde(child->subdir);
  return proc_unregister(child->parent, child->low_ino);
}

/*
 * Return a new dynamically allocated proc_dir_entry. All such entries will
 * be freed when the module is unloaded
 */

proc_dir_entry *
click_new_dynamic_pde()
{
  if (!free_pde) {
    proc_dir_entry *new_pde = kmalloc(sizeof(proc_dir_entry) * 48, GFP_ATOMIC);
    if (!new_pde) return 0;
    new_pde[0].next = all_pde;
    all_pde = new_pde;
    for (int i = 2; i < 48; i++)
      all_pde[i].next = &all_pde[i-1];
    all_pde[1].next = 0;
    free_pde = &all_pde[47];
  }
  proc_dir_entry *result = free_pde;
  free_pde = free_pde->next;
  result->next = 0;
  result->subdir = 0;
  result->low_ino = 0;
  return result;
}

/*
 * Make and register a dynamically allocated proc_dir_entry in a single step
 */

proc_dir_entry *
click_register_new_dynamic_pde(proc_dir_entry *parent,
			     const proc_dir_entry &pattern,
			     int namelen, const char *name, void *data)
{
  proc_dir_entry *pde = click_new_dynamic_pde();
  if (!pde) return 0;
  *pde = pattern;
  if (namelen >= 0) {
    pde->namelen = namelen;
    pde->name = name;
    pde->data = data;
  }
  if (click_register_pde(parent, pde) < 0)
    return click_kill_dynamic_pde(pde);
  else
    return pde;
}

/*
 * Kill a dynamically allocated proc_dir_entry by unregistering and freeing
 * it, and any nested files or directories.
 */

int
click_kill_dynamic_pde(proc_dir_entry *pde)
{
  while (pde->subdir)
    click_kill_dynamic_pde(pde->subdir);
  int result;
  if (pde->low_ino != 0)
    result = proc_unregister(pde->parent, pde->low_ino);
  else
    result = 0;
  pde->next = free_pde;
  free_pde = pde;
  return result;
}

/*
 * Find a proc_dir_entry based on name
 */

proc_dir_entry *
click_find_pde(proc_dir_entry *parent, const String &s)
{
  int len = s.length();
  for (proc_dir_entry *pde = parent->subdir; pde; pde = pde->next)
    if (pde->namelen == len && memcmp(pde->name, s.data(), len) == 0)
      return pde;
  return 0;
}


/*
 * Initialization and cleanup
 */

void
init_click_proc()
{
}

void
cleanup_click_proc()
{
  while (all_pde) {
    proc_dir_entry *next = all_pde->next;
    kfree(all_pde);
    all_pde = next;
  }
}
