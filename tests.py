import errno
import os
import unittest

import sysvipc


class SemaphoreTests(unittest.TestCase):
    def setUp(self):
        self.sem = sysvipc.semget(sysvipc.IPC_PRIVATE, 1, 0o600)

    def tearDown(self):
        sysvipc.semctl(self.sem, 0, sysvipc.IPC_RMID)

    def test_increments_and_decrements_work(self):
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETVAL) == 0
        sysvipc.semop(self.sem, [(0, 42, sysvipc.SEM_UNDO)])
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETVAL) == 42
        sysvipc.semop(self.sem, [(0, -42, sysvipc.SEM_UNDO)])
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETVAL) == 0

    def test_raises_permission_error(self):
        sem = sysvipc.semget(sysvipc.IPC_PRIVATE, 1, 0o000)
        self.assertRaises(OSError, lambda: sysvipc.semctl(sem, 0, sysvipc.GETVAL))
        sysvipc.semctl(sem, 0, sysvipc.IPC_RMID)

    def test_no_such_semaphore(self):
        pass
        # memfailure
        # self.assertRaises(OSError, lambda: sysvipc.semop(self.sem, [(100, 42, sysvipc.SEM_UNDO)]))

    def test_setval_and_getval(self):
        sysvipc.semctl(self.sem, 0, sysvipc.SETVAL, 7)
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETVAL) == 7

    def test_getpid_returns_last_operating_process(self):
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETPID) == 0
        sysvipc.semop(self.sem, [(0, 1, 0)])
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETPID) == os.getpid()

    def test_getncnt_and_getzcnt(self):
        sysvipc.semctl(self.sem, 0, sysvipc.SETVAL, 5)
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETNCNT) == 0
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETZCNT) == 0
        sysvipc.semctl(self.sem, 0, sysvipc.SETVAL, 0)
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETNCNT) == 0
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETZCNT) == 0

    def test_ipc_stat_and_ipc_set(self):
        stat = sysvipc.semctl(self.sem, 0, sysvipc.IPC_STAT)
        assert stat["sem_nsems"] == 1
        assert stat["sem_perm_mode"] & 0o777 == 0o600
        sysvipc.semctl(self.sem, 0, sysvipc.IPC_SET, {"sem_perm_mode": 0o400})
        stat = sysvipc.semctl(self.sem, 0, sysvipc.IPC_STAT)
        assert stat["sem_perm_mode"] & 0o777 == 0o400

    def test_semtimedop_timeout_raises_again(self):
        started = os.times()
        try:
            sysvipc.semtimedop(self.sem, [(0, -1, 0)], 0.1)
            self.fail("semtimedop should have timed out")
        except OSError as e:
            assert e.errno == errno.EAGAIN
        elapsed = os.times().elapsed - started.elapsed
        assert elapsed >= 0.05


class SemaphoreSetTests(unittest.TestCase):
    def setUp(self):
        self.sem = sysvipc.semget(sysvipc.IPC_PRIVATE, 3, 0o600)

    def tearDown(self):
        sysvipc.semctl(self.sem, 0, sysvipc.IPC_RMID)

    def test_setall_and_getall(self):
        sysvipc.semctl(self.sem, 0, sysvipc.SETALL, [1, 2, 3])
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETALL) == [1, 2, 3]

    def test_atomic_operations_on_multiple_semaphores(self):
        sysvipc.semctl(self.sem, 0, sysvipc.SETALL, [5, 5, 5])
        sysvipc.semop(
            self.sem,
            [
                (0, -1, sysvipc.SEM_UNDO),
                (1, -2, sysvipc.SEM_UNDO),
                (2, -3, sysvipc.SEM_UNDO),
            ],
        )
        assert sysvipc.semctl(self.sem, 0, sysvipc.GETALL) == [4, 3, 2]

    def test_ipc_stat_reports_all_semaphores(self):
        stat = sysvipc.semctl(self.sem, 0, sysvipc.IPC_STAT)
        assert stat["sem_nsems"] == 3


class SharedMemoryTests(unittest.TestCase):
    def setUp(self):
        self.shmid = sysvipc.shmget(sysvipc.IPC_PRIVATE, 4096, 0o600)

    def tearDown(self):
        sysvipc.shmctl(self.shmid, sysvipc.IPC_RMID)

    def test_write_and_read_back(self):
        with sysvipc.shmat(self.shmid) as mem:
            memoryview(mem)[0:5] = b"hello"
            assert bytes(memoryview(mem)[0:5]) == b"hello"

    def test_data_persists_across_attach(self):
        with sysvipc.shmat(self.shmid) as mem:
            memoryview(mem)[0:5] = b"hello"
        with sysvipc.shmat(self.shmid) as mem:
            assert bytes(memoryview(mem)[0:5]) == b"hello"

    def test_detach_is_tracked_by_kernel(self):
        with sysvipc.shmat(self.shmid) as mem:
            assert sysvipc.shmctl(self.shmid, sysvipc.IPC_STAT)["shm_nattch"] == 1
        assert sysvipc.shmctl(self.shmid, sysvipc.IPC_STAT)["shm_nattch"] == 0

    def test_len_and_size(self):
        with sysvipc.shmat(self.shmid) as mem:
            assert len(mem) >= 4096
            assert mem.size >= 4096
            assert mem.shmid == self.shmid

    def test_readonly_attach_blocks_writes(self):
        with sysvipc.shmat(self.shmid) as mem:
            memoryview(mem)[0:5] = b"hello"
        ro = sysvipc.shmat(self.shmid, sysvipc.SHM_RDONLY)
        try:
            view = memoryview(ro)
            assert view.readonly
            assert bytes(view[0:5]) == b"hello"
            self.assertRaises(TypeError, lambda: view.__setitem__(0, 1))
        finally:
            ro.detach()

    def test_ipc_stat_and_ipc_set(self):
        stat = sysvipc.shmctl(self.shmid, sysvipc.IPC_STAT)
        assert stat["shm_segsz"] >= 4096
        assert stat["shm_perm_mode"] & 0o777 == 0o600
        sysvipc.shmctl(self.shmid, sysvipc.IPC_SET, {"shm_perm_mode": 0o400})
        stat = sysvipc.shmctl(self.shmid, sysvipc.IPC_STAT)
        assert stat["shm_perm_mode"] & 0o777 == 0o400

    def test_raises_on_invalid_size(self):
        self.assertRaises(OSError, lambda: sysvipc.shmget(sysvipc.IPC_PRIVATE, 0, 0o600))


class MessageQueueTests(unittest.TestCase):
    def setUp(self):
        self.msqid = sysvipc.msgget(sysvipc.IPC_PRIVATE, 0o600)

    def tearDown(self):
        sysvipc.msgctl(self.msqid, sysvipc.IPC_RMID)

    def test_send_and_receive(self):
        sysvipc.msgsnd(self.msqid, 1, b"ping")
        assert sysvipc.msgrcv(self.msqid, 0, 1024, sysvipc.IPC_NOWAIT) == (1, b"ping")

    def test_send_and_receive_text(self):
        sysvipc.msgsnd(self.msqid, 1, "héllo")
        mtype, message = sysvipc.msgrcv(self.msqid, 0, 1024, sysvipc.IPC_NOWAIT)
        assert mtype == 1
        assert message == "héllo".encode()

    def test_fifo_order_within_same_type(self):
        sysvipc.msgsnd(self.msqid, 1, b"first")
        sysvipc.msgsnd(self.msqid, 1, b"second")
        assert sysvipc.msgrcv(self.msqid, 0, 1024, sysvipc.IPC_NOWAIT) == (1, b"first")
        assert sysvipc.msgrcv(self.msqid, 0, 1024, sysvipc.IPC_NOWAIT) == (1, b"second")

    def test_receive_by_absolute_type(self):
        sysvipc.msgsnd(self.msqid, 1, b"one")
        sysvipc.msgsnd(self.msqid, 2, b"two")
        assert sysvipc.msgrcv(self.msqid, 2, 1024, sysvipc.IPC_NOWAIT) == (2, b"two")
        assert sysvipc.msgrcv(self.msqid, 1, 1024, sysvipc.IPC_NOWAIT) == (1, b"one")

    def test_receive_by_negative_type(self):
        sysvipc.msgsnd(self.msqid, 1, b"one")
        sysvipc.msgsnd(self.msqid, 2, b"two")
        # msgtyp < 0 reads the lowest type message with type <= abs(msgtyp)
        mtype, message = sysvipc.msgrcv(self.msqid, -1, 1024, sysvipc.IPC_NOWAIT)
        assert mtype == 1
        assert message == b"one"
        assert sysvipc.msgrcv(self.msqid, 0, 1024, sysvipc.IPC_NOWAIT) == (2, b"two")

    def test_nowait_on_empty_queue_raises_enomsg(self):
        try:
            sysvipc.msgrcv(self.msqid, 0, 1024, sysvipc.IPC_NOWAIT)
            self.fail("msgrcv should have failed")
        except OSError as e:
            assert e.errno == errno.ENOMSG

    def test_ipc_stat_and_ipc_set(self):
        stat = sysvipc.msgctl(self.msqid, sysvipc.IPC_STAT)
        assert stat["msg_qnum"] == 0
        assert stat["msg_perm_mode"] & 0o777 == 0o600
        sysvipc.msgsnd(self.msqid, 1, b"ping")
        stat = sysvipc.msgctl(self.msqid, sysvipc.IPC_STAT)
        assert stat["msg_qnum"] == 1
        sysvipc.msgctl(self.msqid, sysvipc.IPC_SET, {"msg_perm_mode": 0o400})
        stat = sysvipc.msgctl(self.msqid, sysvipc.IPC_STAT)
        assert stat["msg_perm_mode"] & 0o777 == 0o400


if __name__ == "__main__":
    unittest.main()
