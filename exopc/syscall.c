#include <xok/sys_ucall.h>


int
xok_sys_self_dpf_insert (u_int a1, u_int a2, void *a3, int a4)
{
  return sys_self_dpf_insert(a1, a2, a3, a4);
}


int
xok_sys_self_dpf_ref(u_int a1, u_int a2)
{
  return sys_self_dpf_ref(a1, a2);
}


int
xok_sys_self_dpf_delete (u_int a1, u_int a2)
{
  return sys_self_dpf_delete(a1, a2);
}


int
vos_prd_ring_id(int fd)
{
  extern int prd_ring_id(int);
  return prd_ring_id(fd);
}

