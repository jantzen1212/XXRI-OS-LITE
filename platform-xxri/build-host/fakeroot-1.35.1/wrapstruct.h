/* Automatically generated file. Do not edit. Edit wrapawk/wrapfunc.inp. */
#ifndef WRAPSTRUCT_H
#define WRAPSTRUCT_H
struct next_wrap_st next_wrap[]= {


  {(void(*))&NEXT_LSTAT_NOARG, WRAP_LSTAT_QUOTE},
  {(void(*))&NEXT_STAT_NOARG, WRAP_STAT_QUOTE},
  {(void(*))&NEXT_FSTAT_NOARG, WRAP_FSTAT_QUOTE},
#ifdef HAVE_FSTATAT
  {(void(*))&NEXT_FSTATAT_NOARG, WRAP_FSTATAT_QUOTE},
#endif /* HAVE_FSTATAT */

#ifdef STAT64_SUPPORT
  {(void(*))&NEXT_LSTAT64_NOARG, WRAP_LSTAT64_QUOTE},
  {(void(*))&NEXT_STAT64_NOARG, WRAP_STAT64_QUOTE},
  {(void(*))&NEXT_FSTAT64_NOARG, WRAP_FSTAT64_QUOTE},
#ifdef HAVE_FSTATAT
  {(void(*))&NEXT_FSTATAT64_NOARG, WRAP_FSTATAT64_QUOTE},
#endif /* HAVE_FSTATAT */
#endif /* STAT64_SUPPORT */

#ifdef __APPLE__
#ifdef __LP64__
  {(void(*))&next_getattrlist, "getattrlist"},
#ifdef HAVE_FGETATTRLIST
  {(void(*))&next_fgetattrlist, "fgetattrlist"},
#endif
#else
  {(void(*))&next_getattrlist, "getattrlist"},
#ifdef HAVE_FGETATTRLIST
  {(void(*))&next_fgetattrlist, "fgetattrlist"},
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
  {(void(*))&next_getattrlist$UNIX2003, "getattrlist$UNIX2003"},
#endif
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
#include <spawn.h>
  {(void(*))&next_lstat$INODE64, "lstat$INODE64"},
  {(void(*))&next_stat$INODE64, "stat$INODE64"},
  {(void(*))&next_fstat$INODE64, "fstat$INODE64"},
  {(void(*))&next_posix_spawn, "posix_spawn"},
  {(void(*))&next_posix_spawnp, "posix_spawnp"},
#endif
  {(void(*))&next_execve, "execve"},
  {(void(*))&next_execv, "execv"},
  {(void(*))&next_execvp, "execvp"},
  {(void(*))&next_execvP, "execvP"},
#endif /* ifdef __APPLE__ */

  {(void(*))&NEXT_MKNOD_NOARG, WRAP_MKNOD_QUOTE},

#ifdef HAVE_FSTATAT
#ifdef HAVE_MKNODAT
  {(void(*))&NEXT_MKNODAT_NOARG, WRAP_MKNODAT_QUOTE},
#endif /* HAVE_MKNODAT */
#endif /* HAVE_FSTATAT */


  {(void(*))&next_chown, "chown"},
  {(void(*))&next_lchown, "lchown"},
  {(void(*))&next_fchown, "fchown"},
  {(void(*))&next_chmod, "chmod"},
  {(void(*))&next_fchmod, "fchmod"},
#if defined HAVE_LCHMOD
  {(void(*))&next_lchmod, "lchmod"},
#endif
#if defined __APPLE__ && !defined __LP64__
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
  {(void(*))&next_lchown$UNIX2003, "lchown$UNIX2003"},
  {(void(*))&next_chmod$UNIX2003, "chmod$UNIX2003"},
  {(void(*))&next_fchmod$UNIX2003, "fchmod$UNIX2003"},
#endif
#endif /* if defined __APPLE__ && !defined __LP64__ */
  {(void(*))&next_mkdir, "mkdir"},
  {(void(*))&next_unlink, "unlink"},
  {(void(*))&next_rmdir, "rmdir"},
  {(void(*))&next_remove, "remove"},
  {(void(*))&next_rename, "rename"},

#ifdef FAKEROOT_FAKENET
  {(void(*))&next_fork, "fork"},
  {(void(*))&next_vfork, "vfork"},
  {(void(*))&next_close, "close"},
  {(void(*))&next_dup2, "dup2"},
#endif /* FAKEROOT_FAKENET */


  {(void(*))&next_getuid, "getuid"},
  {(void(*))&next_getgid, "getgid"},
  {(void(*))&next_geteuid, "geteuid"},
  {(void(*))&next_getegid, "getegid"},
  {(void(*))&next_setuid, "setuid"},
  {(void(*))&next_setgid, "setgid"},
  {(void(*))&next_seteuid, "seteuid"},
  {(void(*))&next_setegid, "setegid"},
  {(void(*))&next_setreuid, "setreuid"},
  {(void(*))&next_setregid, "setregid"},
#if defined __APPLE__ && !defined __LP64__
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
  {(void(*))&next_setreuid$UNIX2003, "setreuid$UNIX2003"},
  {(void(*))&next_setregid$UNIX2003, "setregid$UNIX2003"},
#endif
#endif /* if defined __APPLE__ && !defined __LP64__ */
#ifdef HAVE_GETRESUID
  {(void(*))&next_getresuid, "getresuid"},
#endif /* HAVE_GETRESUID */
#ifdef HAVE_GETRESGID
  {(void(*))&next_getresgid, "getresgid"},
#endif /* HAVE_GETRESGID */
#ifdef HAVE_SETRESUID
  {(void(*))&next_setresuid, "setresuid"},
#endif /* HAVE_SETRESUID */
#ifdef HAVE_SETRESGID
  {(void(*))&next_setresgid, "setresgid"},
#endif /* HAVE_SETRESGID */
#ifdef HAVE_SETFSUID
  {(void(*))&next_setfsuid, "setfsuid"},
#endif /* HAVE_SETFSUID */
#ifdef HAVE_SETFSGID
  {(void(*))&next_setfsgid, "setfsgid"},
#endif /* HAVE_SETFSGID */
  {(void(*))&next_initgroups, "initgroups"},
  {(void(*))&next_getgroups, "getgroups"},
  {(void(*))&next_setgroups, "setgroups"},
  {(void(*))&next_setpriority, "setpriority"},
#ifdef HAVE_CAPSET
  {(void(*))&next_capset, "capset"},
#endif /* HAVE_CAPSET */
#ifdef HAVE_LISTXATTR
  {(void(*))&next_listxattr, "listxattr"},
#endif /* HAVE_LISTXATTR */
#ifdef HAVE_LLISTXATTR
  {(void(*))&next_llistxattr, "llistxattr"},
#endif /* HAVE_LLISTXATTR */
#ifdef HAVE_FLISTXATTR
  {(void(*))&next_flistxattr, "flistxattr"},
#endif /* HAVE_FLISTXATTR */
#ifdef HAVE_GETXATTR
  {(void(*))&next_getxattr, "getxattr"},
#endif /* HAVE_GETXATTR */
#ifdef HAVE_LGETXATTR
  {(void(*))&next_lgetxattr, "lgetxattr"},
#endif /* HAVE_LGETXATTR */
#ifdef HAVE_FGETXATTR
  {(void(*))&next_fgetxattr, "fgetxattr"},
#endif /* HAVE_FGETXATTR */
#ifdef HAVE_SETXATTR
  {(void(*))&next_setxattr, "setxattr"},
#endif /* HAVE_SETXATTR */
#ifdef HAVE_LSETXATTR
  {(void(*))&next_lsetxattr, "lsetxattr"},
#endif /* HAVE_LSETXATTR */
#ifdef HAVE_FSETXATTR
  {(void(*))&next_fsetxattr, "fsetxattr"},
#endif /* HAVE_FSETXATTR */
#ifdef HAVE_REMOVEXATTR
  {(void(*))&next_removexattr, "removexattr"},
#endif /* HAVE_REMOVEXATTR */
#ifdef HAVE_LREMOVEXATTR
  {(void(*))&next_lremovexattr, "lremovexattr"},
#endif /* HAVE_LREMOVEXATTR */
#ifdef HAVE_FREMOVEXATTR
  {(void(*))&next_fremovexattr, "fremovexattr"},
#endif /* HAVE_FREMOVEXATTR */

#ifdef HAVE_FSTATAT
#ifdef HAVE_FCHMODAT
  {(void(*))&next_fchmodat, "fchmodat"},
#endif /* HAVE_FCHMODAT */
#ifdef HAVE_FCHOWNAT
  {(void(*))&next_fchownat, "fchownat"},
#endif /* HAVE_FCHOWNAT */
#ifdef HAVE_MKDIRAT
  {(void(*))&next_mkdirat, "mkdirat"},
#endif /* HAVE_MKDIRAT */
#ifdef HAVE_RENAMEAT
  {(void(*))&next_renameat, "renameat"},
#endif /* HAVE_RENAMEAT */
#ifdef HAVE_RENAMEAT2
  {(void(*))&next_renameat2, "renameat2"},
#endif /* HAVE_RENAMEAT2 */
#ifdef HAVE_UNLINKAT
  {(void(*))&next_unlinkat, "unlinkat"},
#endif /* HAVE_UNLINKAT */
#endif /* HAVE_FSTATAT */

#ifdef HAVE_ACL_T

#ifdef HAVE_ACL_GET_FD
  {(void(*))&next_acl_get_fd, "acl_get_fd"},
  {(void(*))&next_acl_get_file, "acl_get_file"},
  {(void(*))&next_acl_set_fd, "acl_set_fd"},
  {(void(*))&next_acl_set_file, "acl_set_file"},
#endif

#ifdef HAVE_ACL_TRIVIAL
  {(void(*))&next_acl_get, "acl_get"},
  {(void(*))&next_facl_get, "facl_get"},
  {(void(*))&next_acl_set, "acl_set"},
  {(void(*))&next_facl_set, "facl_set"},
  {(void(*))&next_acl_trivial, "acl_trivial"},
  {(void(*))&next_acl, "acl"},
  {(void(*))&next_facl, "facl"},
#endif

#endif /* HAVE_ACL_T */

#ifdef HAVE_FTS_READ
  {(void(*))&next_fts_read, "fts_read"},
#ifdef __APPLE__
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
  {(void(*))&next_fts_read$INODE64, "fts_read$INODE64"},
#endif
#endif /* ifdef __APPLE__ */
#endif /* HAVE_FTS_READ */
#ifdef HAVE_FTS_CHILDREN
  {(void(*))&next_fts_children, "fts_children"},
#ifdef __APPLE__
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
  {(void(*))&next_fts_children$INODE64, "fts_children$INODE64"},
#endif
#endif /* ifdef __APPLE__ */
#endif /* HAVE_FTS_CHILDREN */

#ifdef HAVE_STATX
  {(void(*))&next_statx, "statx"},
#endif /* HAVE_STATX */

#ifdef __sun
  {(void(*))&next_sysinfo, "sysinfo"},
#endif

#ifdef TIME64_HACK
  {(void(*))&NEXT_LSTAT64_TIME64_NOARG, WRAP_LSTAT64_TIME64_QUOTE},
  {(void(*))&NEXT_STAT64_TIME64_NOARG, WRAP_STAT64_TIME64_QUOTE},
  {(void(*))&NEXT_FSTAT64_TIME64_NOARG, WRAP_FSTAT64_TIME64_QUOTE},
  {(void(*))&NEXT_FSTATAT64_TIME64_NOARG, WRAP_FSTATAT64_TIME64_QUOTE},
#endif /* TIME64_HACK */
  {NULL, NULL},
};
#endif
