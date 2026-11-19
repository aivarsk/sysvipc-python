#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <ctime>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace py = pybind11;

static void throw_errno() {
  PyErr_SetFromErrno(PyExc_OSError);
  throw pybind11::error_already_set();
}

union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
};

static int do_semop(int semid, struct sembuf *ops, std::size_t nops) {
  py::gil_scoped_release release;
  return semop(semid, ops, nops);
}

static int do_semtimedop(int semid, struct sembuf *ops, std::size_t nops,
                         const struct timespec *ts) {
  py::gil_scoped_release release;
  return semtimedop(semid, ops, nops, ts);
}

static int do_msgsnd(int msqid, const void *msgp, std::size_t msgsz,
                     int msgflg) {
  py::gil_scoped_release release;
  return msgsnd(msqid, msgp, msgsz, msgflg);
}

static ssize_t do_msgrcv(int msqid, void *msgp, std::size_t msgsz, long msgtyp,
                         int msgflg) {
  py::gil_scoped_release release;
  return msgrcv(msqid, msgp, msgsz, msgtyp, msgflg);
}

static py::dict semid_ds_to_dict(const struct semid_ds &ds) {
  py::dict d;
  d["sem_perm_uid"] = (unsigned long)ds.sem_perm.uid;
  d["sem_perm_gid"] = (unsigned long)ds.sem_perm.gid;
  d["sem_perm_cuid"] = (unsigned long)ds.sem_perm.cuid;
  d["sem_perm_cgid"] = (unsigned long)ds.sem_perm.cgid;
  d["sem_perm_mode"] = (unsigned int)ds.sem_perm.mode;
  d["sem_otime"] = (long)ds.sem_otime;
  d["sem_ctime"] = (long)ds.sem_ctime;
  d["sem_nsems"] = (unsigned long)ds.sem_nsems;
  return d;
}

static void semid_ds_from_dict(struct semid_ds &ds, const py::dict &d) {
  if (d.contains("sem_perm_uid")) ds.sem_perm.uid = d["sem_perm_uid"].cast<uid_t>();
  if (d.contains("sem_perm_gid")) ds.sem_perm.gid = d["sem_perm_gid"].cast<gid_t>();
  if (d.contains("sem_perm_mode")) ds.sem_perm.mode = d["sem_perm_mode"].cast<unsigned short>();
}

static py::dict msqid_ds_to_dict(const struct msqid_ds &ds) {
  py::dict d;
  d["msg_perm_uid"] = (unsigned long)ds.msg_perm.uid;
  d["msg_perm_gid"] = (unsigned long)ds.msg_perm.gid;
  d["msg_perm_cuid"] = (unsigned long)ds.msg_perm.cuid;
  d["msg_perm_cgid"] = (unsigned long)ds.msg_perm.cgid;
  d["msg_perm_mode"] = (unsigned int)ds.msg_perm.mode;
  d["msg_stime"] = (long)ds.msg_stime;
  d["msg_rtime"] = (long)ds.msg_rtime;
  d["msg_ctime"] = (long)ds.msg_ctime;
  d["msg_qnum"] = (unsigned long)ds.msg_qnum;
  d["msg_qbytes"] = (unsigned long)ds.msg_qbytes;
  d["msg_lspid"] = (long)ds.msg_lspid;
  d["msg_lrpid"] = (long)ds.msg_lrpid;
  return d;
}

static void msqid_ds_from_dict(struct msqid_ds &ds, const py::dict &d) {
  if (d.contains("msg_perm_uid")) ds.msg_perm.uid = d["msg_perm_uid"].cast<uid_t>();
  if (d.contains("msg_perm_gid")) ds.msg_perm.gid = d["msg_perm_gid"].cast<gid_t>();
  if (d.contains("msg_perm_mode")) ds.msg_perm.mode = d["msg_perm_mode"].cast<unsigned short>();
  if (d.contains("msg_qbytes")) ds.msg_qbytes = d["msg_qbytes"].cast<unsigned long>();
}

static py::dict shmid_ds_to_dict(const struct shmid_ds &ds) {
  py::dict d;
  d["shm_perm_uid"] = (unsigned long)ds.shm_perm.uid;
  d["shm_perm_gid"] = (unsigned long)ds.shm_perm.gid;
  d["shm_perm_cuid"] = (unsigned long)ds.shm_perm.cuid;
  d["shm_perm_cgid"] = (unsigned long)ds.shm_perm.cgid;
  d["shm_perm_mode"] = (unsigned int)ds.shm_perm.mode;
  d["shm_segsz"] = (unsigned long)ds.shm_segsz;
  d["shm_atime"] = (long)ds.shm_atime;
  d["shm_dtime"] = (long)ds.shm_dtime;
  d["shm_ctime"] = (long)ds.shm_ctime;
  d["shm_cpid"] = (long)ds.shm_cpid;
  d["shm_lpid"] = (long)ds.shm_lpid;
  d["shm_nattch"] = (unsigned long)ds.shm_nattch;
  return d;
}

static void shmid_ds_from_dict(struct shmid_ds &ds, const py::dict &d) {
  if (d.contains("shm_perm_uid")) ds.shm_perm.uid = d["shm_perm_uid"].cast<uid_t>();
  if (d.contains("shm_perm_gid")) ds.shm_perm.gid = d["shm_perm_gid"].cast<gid_t>();
  if (d.contains("shm_perm_mode")) ds.shm_perm.mode = d["shm_perm_mode"].cast<unsigned short>();
}

struct shm {
  int shmid;
  void *addr;
  std::size_t size;
  bool readonly;

  void detach() {
    if (addr == nullptr) return;
    if (shmdt(addr) == -1) throw_errno();
    addr = nullptr;
  }

  ~shm() {
    if (addr != nullptr) shmdt(addr);
  }
};

PYBIND11_MODULE(sysvipc, m) {
  py::class_<shm>(m, "shm", py::buffer_protocol())
      .def_buffer([](shm &self) {
        return py::buffer_info(self.addr, sizeof(char), "B", 1, {self.size},
                               {sizeof(char)}, self.readonly);
      })
      .def("detach", &shm::detach)
      .def("__enter__", [](shm &self) -> shm & { return self; })
      .def("__exit__", [](shm &self, py::args) { self.detach(); })
      .def("__len__", [](shm &self) { return self.size; })
      .def_readonly("shmid", &shm::shmid)
      .def_readonly("size", &shm::size);

  m.def(
      "semget",
      [](int key, int nsems, int semflg) {
        int result = semget((key_t)key, nsems, semflg);
        if (result == -1) {
          throw_errno();
        }
        return result;
      },
      "Get a System V semaphore set identifier", py::arg("key"),
      py::arg("nsems"), py::arg("semflg"));

  m.def(
      "semop",
      [](int semid, const std::vector<std::tuple<short, short, short>> &sops) {
        std::vector<sembuf> csemops(sops.size());
        for (unsigned i = 0, max = sops.size(); i < max; ++i) {
          csemops[i].sem_num = std::get<0>(sops[i]);
          csemops[i].sem_op = std::get<1>(sops[i]);
          csemops[i].sem_flg = std::get<2>(sops[i]);
        }
        int result = do_semop(semid, csemops.data(), csemops.size());
        if (result == -1) {
          throw_errno();
        }
        return result;
      },
      "System V semaphore operations", py::arg("semid"), py::arg("sops"));

  m.def(
      "semtimedop",
      [](int semid, const std::vector<std::tuple<short, short, short>> &sops,
         double timeout) {
        std::vector<sembuf> csemops(sops.size());
        for (unsigned i = 0, max = sops.size(); i < max; ++i) {
          csemops[i].sem_num = std::get<0>(sops[i]);
          csemops[i].sem_op = std::get<1>(sops[i]);
          csemops[i].sem_flg = std::get<2>(sops[i]);
        }
        struct timespec ts;
        ts.tv_sec = (time_t)timeout;
        ts.tv_nsec = (long)((timeout - (double)ts.tv_sec) * 1e9);
        int result = do_semtimedop(semid, csemops.data(), csemops.size(), &ts);
        if (result == -1) {
          throw_errno();
        }
        return result;
      },
      "System V semaphore operations with a timeout", py::arg("semid"),
      py::arg("sops"), py::arg("timeout"));

  m.def(
      "semctl",
      [](int semid, int semnum, int cmd, py::object arg) -> py::object {
        switch (cmd) {
          case IPC_RMID: {
            int result = semctl(semid, semnum, IPC_RMID);
            if (result == -1) throw_errno();
            return py::int_(result);
          }
          case GETVAL:
          case GETPID:
          case GETNCNT:
          case GETZCNT: {
            int result = semctl(semid, semnum, cmd);
            if (result == -1) throw_errno();
            return py::int_(result);
          }
          case SETVAL: {
            int result = semctl(semid, semnum, SETVAL, arg.cast<int>());
            if (result == -1) throw_errno();
            return py::int_(result);
          }
          case SETALL: {
            std::vector<unsigned short> values =
                arg.cast<std::vector<unsigned short>>();
            union semun s;
            s.array = values.data();
            int result = semctl(semid, semnum, SETALL, s);
            if (result == -1) throw_errno();
            return py::int_(result);
          }
          case GETALL: {
            struct semid_ds ds;
            union semun s;
            s.buf = &ds;
            if (semctl(semid, semnum, IPC_STAT, s) == -1) throw_errno();
            std::vector<unsigned short> values(ds.sem_nsems);
            s.array = values.data();
            if (semctl(semid, semnum, GETALL, s) == -1) throw_errno();
            py::list result;
            for (unsigned short v : values) {
              result.append(py::int_(v));
            }
            return result;
          }
          case IPC_STAT: {
            struct semid_ds ds;
            union semun s;
            s.buf = &ds;
            if (semctl(semid, semnum, IPC_STAT, s) == -1) throw_errno();
            return semid_ds_to_dict(ds);
          }
          case IPC_SET: {
            struct semid_ds ds;
            union semun s;
            s.buf = &ds;
            if (semctl(semid, semnum, IPC_STAT, s) == -1) throw_errno();
            semid_ds_from_dict(ds, arg.cast<py::dict>());
            if (semctl(semid, semnum, IPC_SET, s) == -1) throw_errno();
            return py::int_(0);
          }
          default:
            throw py::value_error("unsupported semctl command");
        }
      },
      "System V semaphore control operations", py::arg("semid"),
      py::arg("semnum"), py::arg("cmd"), py::arg("arg") = py::none());

  m.def(
      "msgget",
      [](int key, int msgflg) {
        int result = msgget((key_t)key, msgflg);
        if (result == -1) {
          throw_errno();
        }
        return result;
      },
      "Get a System V message queue identifier", py::arg("key"),
      py::arg("msgflg"));

  m.def(
      "msgsnd",
      [](int msqid, long mtype, const std::string &message, int msgflg) {
        std::vector<char> buffer(sizeof(long) + message.size());
        std::memcpy(buffer.data(), &mtype, sizeof(long));
        std::memcpy(buffer.data() + sizeof(long), message.data(),
                    message.size());
        int result = do_msgsnd(msqid, buffer.data(), message.size(), msgflg);
        if (result == -1) {
          throw_errno();
        }
        return result;
      },
      "Send a message on a System V message queue", py::arg("msqid"),
      py::arg("mtype"), py::arg("message"), py::arg("msgflg") = 0);

  m.def(
      "msgrcv",
      [](int msqid, long msgtyp, long msgsz, int msgflg) {
        if (msgsz < 0) {
          struct msqid_ds ds;
          if (msgctl(msqid, IPC_STAT, &ds) == -1) {
            throw_errno();
          }
          msgsz = ds.msg_qbytes;
        }
        std::vector<char> buffer(sizeof(long) + msgsz);
        ssize_t result =
            do_msgrcv(msqid, buffer.data(), msgsz, msgtyp, msgflg);
        if (result == -1) {
          throw_errno();
        }
        long mtype;
        std::memcpy(&mtype, buffer.data(), sizeof(long));
        return py::make_tuple(mtype,
                              py::bytes(buffer.data() + sizeof(long), result));
      },
      "Receive a message from a System V message queue, returns a tuple of "
      "(mtype, message)",
      py::arg("msqid"), py::arg("msgtyp") = 0, py::arg("msgsz") = -1,
      py::arg("msgflg") = 0);

  m.def(
      "msgctl",
      [](int msqid, int cmd, py::object arg) -> py::object {
        switch (cmd) {
          case IPC_RMID: {
            int result = msgctl(msqid, IPC_RMID, nullptr);
            if (result == -1) throw_errno();
            return py::int_(result);
          }
          case IPC_STAT: {
            struct msqid_ds ds;
            if (msgctl(msqid, IPC_STAT, &ds) == -1) throw_errno();
            return msqid_ds_to_dict(ds);
          }
          case IPC_SET: {
            struct msqid_ds ds;
            if (msgctl(msqid, IPC_STAT, &ds) == -1) throw_errno();
            msqid_ds_from_dict(ds, arg.cast<py::dict>());
            if (msgctl(msqid, IPC_SET, &ds) == -1) throw_errno();
            return py::int_(0);
          }
          default:
            throw py::value_error("unsupported msgctl command");
        }
      },
      "System V message queue control operations", py::arg("msqid"),
      py::arg("cmd"), py::arg("arg") = py::none());

  m.def(
      "shmget",
      [](int key, std::size_t size, int shmflg) {
        int result = shmget((key_t)key, size, shmflg);
        if (result == -1) {
          throw_errno();
        }
        return result;
      },
      "Allocate a System V shared memory segment", py::arg("key"),
      py::arg("size"), py::arg("shmflg"));

  m.def(
      "shmat",
      [](int shmid, int shmflg) {
        struct shmid_ds ds;
        if (shmctl(shmid, IPC_STAT, &ds) == -1) {
          throw_errno();
        }
        void *addr = shmat(shmid, nullptr, shmflg);
        if (addr == (void *)-1) {
          throw_errno();
        }
        auto self = std::make_unique<shm>();
        self->shmid = shmid;
        self->addr = addr;
        self->size = (std::size_t)ds.shm_segsz;
        self->readonly = (shmflg & SHM_RDONLY) != 0;
        return self;
      },
      "Attach a shared memory segment, returns a buffer-like object",
      py::arg("shmid"), py::arg("shmflg") = 0);

  m.def(
      "shmctl",
      [](int shmid, int cmd, py::object arg) -> py::object {
        switch (cmd) {
          case IPC_RMID: {
            int result = shmctl(shmid, IPC_RMID, nullptr);
            if (result == -1) throw_errno();
            return py::int_(result);
          }
          case IPC_STAT: {
            struct shmid_ds ds;
            if (shmctl(shmid, IPC_STAT, &ds) == -1) throw_errno();
            return shmid_ds_to_dict(ds);
          }
          case IPC_SET: {
            struct shmid_ds ds;
            if (shmctl(shmid, IPC_STAT, &ds) == -1) throw_errno();
            shmid_ds_from_dict(ds, arg.cast<py::dict>());
            if (shmctl(shmid, IPC_SET, &ds) == -1) throw_errno();
            return py::int_(0);
          }
          default:
            throw py::value_error("unsupported shmctl command");
        }
      },
      "System V shared memory control operations", py::arg("shmid"),
      py::arg("cmd"), py::arg("arg") = py::none());

  m.attr("IPC_PRIVATE") = py::int_(IPC_PRIVATE);
  m.attr("IPC_CREAT") = py::int_(IPC_CREAT);
  m.attr("IPC_EXCL") = py::int_(IPC_EXCL);
  m.attr("IPC_RMID") = py::int_(IPC_RMID);
  m.attr("IPC_NOWAIT") = py::int_(IPC_NOWAIT);
  m.attr("IPC_STAT") = py::int_(IPC_STAT);
  m.attr("IPC_SET") = py::int_(IPC_SET);

  m.attr("GETVAL") = py::int_(GETVAL);
  m.attr("SETVAL") = py::int_(SETVAL);
  m.attr("GETPID") = py::int_(GETPID);
  m.attr("GETNCNT") = py::int_(GETNCNT);
  m.attr("GETZCNT") = py::int_(GETZCNT);
  m.attr("GETALL") = py::int_(GETALL);
  m.attr("SETALL") = py::int_(SETALL);
  m.attr("SEM_UNDO") = py::int_(SEM_UNDO);

  m.attr("MSG_NOERROR") = py::int_(MSG_NOERROR);

  m.attr("SHM_RDONLY") = py::int_(SHM_RDONLY);
  m.attr("SHM_RND") = py::int_(SHM_RND);

  m.doc() =
      R"(Python bindings for System V IPC: semaphores, message queues, and shared memory.

Thin wrappers around semget/semop/semctl, msgget/msgsnd/msgrcv/msgctl, and shmget/shmat/shmctl.
Syscall failures raise OSError with the errno set by the kernel.

Constants:

semget/msgget/shmget flags:
- IPC_PRIVATE - create a new semaphore set, message queue, or shared memory segment.
- IPC_CREAT - create the resource if it does not exist.
- IPC_EXCL - fail if the resource already exists.

semop/semtimedop flags:
- SEM_UNDO - the operation is automatically undone when the process terminates.
- IPC_NOWAIT - fail with EAGAIN instead of blocking.

semctl/msgctl/shmctl commands:
- IPC_RMID - immediately remove the resource.
- IPC_STAT - return the resource status as a dict.
- IPC_SET - update the resource status from a dict.

semctl semaphore commands:
- GETVAL/SETVAL - read/set the value of a single semaphore.
- GETALL/SETALL - read/set the values of all semaphores in the set.
- GETPID/GETNCNT/GETZCNT - read the pid of the last operation and the number of waiting processes.

msgrcv flags:
- MSG_NOERROR - truncate messages longer than msgsz instead of failing with E2BIG.

shmat flags:
- SHM_RDONLY - attach the segment for reading only.
- SHM_RND - round the attach address down to a multiple of SHMLBA (unused, the kernel picks the address).
)";
}
