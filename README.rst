=======================
System V IPC primitives
=======================

Python bindings for System V IPC: semaphores, message queues, and shared memory.

Thin wrappers around the syscalls. Failures raise ``OSError`` with the kernel errno.

Examples:

.. code:: Python

  import atexit
  import sysvipc

  # Semaphores
  sem = sysvipc.semget(sysvipc.IPC_PRIVATE, 1, 0o600)
  atexit.register(lambda: sysvipc.semctl(sem, 0, sysvipc.IPC_RMID))

  sysvipc.semop(sem, [(0, 1, sysvipc.SEM_UNDO)])

  curval = sysvipc.semctl(sem, 0, sysvipc.GETVAL)

  sysvipc.semop(sem, [(0, -1, sysvipc.SEM_UNDO)])

  # Message queues
  msqid = sysvipc.msgget(sysvipc.IPC_PRIVATE, 0o600)
  atexit.register(lambda: sysvipc.msgctl(msqid, sysvipc.IPC_RMID))

  sysvipc.msgsnd(msqid, 1, "hello")
  mtype, message = sysvipc.msgrcv(msqid, 0, 1024)

  # Shared memory
  shmid = sysvipc.shmget(sysvipc.IPC_PRIVATE, 4096, 0o600)
  atexit.register(lambda: sysvipc.shmctl(shmid, sysvipc.IPC_RMID))

  with sysvipc.shmat(shmid) as mem:
      memoryview(mem)[0:5] = b"hello"
      print(bytes(memoryview(mem)[0:5]))
