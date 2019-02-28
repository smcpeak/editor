// This comes from riscv-fesvr/fesvr/syscall.cc.
// The macro definitions caused a problem at one point.

reg_t syscall_t::sys_pselect6(reg_t nfds, reg_t client_readfds_ptr, reg_t client_writefds_ptr, reg_t client_exceptfds_ptr, reg_t client_timespec_ptr, reg_t client_sigmask_ptr, reg_t a6) {

  // This macro operates on the assumption that fd_set, timespec, and sigset_t
  // will be of the same size on the client and the host. This is actually a
  // prett safe assumption, as fd_set is always a bitvector large enough to
  // describe 1024 file descriptors, sigset_t is a bitvector describing 128
  // signals, and timespec is always a struct of two longs. As long as we are
  // using a 64 bit system on both ends, this should just work.
#define TRANSFER_FROM_CLIENT_FOR_PSELECT(name, type) \
  type host_## name; \
  type *host_ ## name ##_ptr = \
    (client_ ## name ## _ptr) ? \
      ({ memif->read(client_ ## name ## _ptr, sizeof(type), &host_ ## name); \
         &host_ ## name; }) : \
      NULL; \

#define TRANSFER_FROM_CLIENT_AND_TRANSLATE_FOR_PSELECT(name) \
  fd_set host_ ## name; \
  fd_set translated_host_ ## name; \
  fd_set *translated_host_ ## name ## _ptr = (client_ ## name ## _ptr) ? \
    ({ \
     memif->read(client_ ## name ## _ptr, sizeof(fd_set), &host_ ## name); \
     translate_fd_sets(fds, &translated_host_ ## name, &host_ ## name); \
       &translated_host_ ## name; }) : NULL;

  TRANSFER_FROM_CLIENT_AND_TRANSLATE_FOR_PSELECT(readfds);
  TRANSFER_FROM_CLIENT_AND_TRANSLATE_FOR_PSELECT(writefds);
  TRANSFER_FROM_CLIENT_AND_TRANSLATE_FOR_PSELECT(exceptfds);
  TRANSFER_FROM_CLIENT_FOR_PSELECT(timespec, timespec);
  TRANSFER_FROM_CLIENT_FOR_PSELECT(sigmask, sigset_t);

#undef TRANFER_FROM_CLIENT_FOR_PSELECT
#undef TRANSFER_FROM_CLIENT_AND_TRANSLATE_FOR_PSELECT

  return sysret_errno(pselect
      (nfds,
       translated_host_readfds_ptr,
       translated_host_writefds_ptr,
       translated_host_exceptfds_ptr,
       host_timespec_ptr,
       host_sigmask_ptr));
}
