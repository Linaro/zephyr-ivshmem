IVSHMEM dual test bed sample app
################################

Prerequisites
*************

QEMU needs to available.

ivshmem-server needs to be available and running. The server is available in
Zephyr SDK or pre-built in some distributions. Otherwise, it is available in
QEMU source tree.

ivshmem-client needs to be available as it is employed in this sample as an
external application. The same conditions of ivshmem-server applies to the
ivshmem-server, as it is also available via QEMU.

Also this test-bed assumes upstream Zephyr branch.

Preparing IVSHMEM server
************************
#. The ivshmem-server utillity for QEMU can be found into the zephyr sdk
   directory, in:
   ``/path/to/your/zephyr-sdk/zephyr-<version>/sysroots/x86_64-pokysdk-linux/usr/xilinx/bin/``

#. You may also find ivshmem-client utillity, it can be useful to debug if everything works
   as expected.

#. Run ivshmem-server. For the ivshmem-server, both number of vectors and
   shared memory size are decided at run-time (when the server is executed).
   For Zephyr, the number of vectors and shared memory size of ivshmem are
   decided at compile-time and run-time, respectively.For Arm64 we use
   vectors == 2 for the project configuration in this sample. Here is an example:

   .. code-block:: console

      # n = number of vectors
      $ sudo ivshmem-server -n 2
      $ *** Example code, do not use in production ***

#. Appropriately set ownership of ``/dev/shm/ivshmem`` and
   ``/tmp/ivshmem_socket`` for your deployment scenario. For instance:

   .. code-block:: console

      $ sudo chgrp $USER /dev/shm/ivshmem
      $ sudo chmod 060 /dev/shm/ivshmem
      $ sudo chgrp $USER /tmp/ivshmem_socket
      $ sudo chmod 060 /tmp/ivshmem_socket

Building and Running
********************
There are instance_1 and instance_2 folders, use the --build-dir or -d options to get two
IVSHMEM Zephyr enabled apps:

   .. code-block:: console

      $ cd path/to/ivshmem_test_bed
      $ west build -pauto -bqemu_cortex_a53 -d instance_1
      $ west build -pauto -bqemu_cortex_a53 -d instance_2

After getting the both appplications built, open two terminals and run each
instance separately:

For example to run instance 1:

   .. code-block:: console

      $ west build -t run -d instance_1

For instance 2, just go to the other build directory in another terminal:

   .. code-block:: console

      $ west build -t run -d instance_2


Using this test-bed
*******************

After running both the instances, you may able to get the following
screenshot in the console:

   .. code-block:: console

      [0/1] To exit from QEMU enter: 'CTRL+a, x'[QEMU] CPU: cortex-a53
      *** Booting Zephyr OS build zephyr-v3.3.0-3373-gca6c08f960d8 ***
      Use ivshmem_send_notify to send a message


      uart:~$

Get the ID of your peers using the dump command:

   .. code-block:: console

      uart:~$ ivshmem_dump shmem
      IVshmem up and running:
            Shared memory: 0xafa00000 of size 4194304 bytes
            Peer id: 11
            Notification vectors: 2

On the other instance running on other terminal tab:

   .. code-block:: console

      uart:~$ ivshmem_dump shmem
      IVshmem up and running:
            Shared memory: 0xafa00000 of size 4194304 bytes
            Peer id: 10
            Notification vectors: 2

Send any data from a peer to another one:

   .. code-block:: console

      uart:~$ ivshmem_send_notify 11 0 "message from peer 10 to peer 11"
      Message message from peer 10 to peer 11 sent to peer 11 on vector 0

Check the result on the other instance:

   .. code-block:: console

      uart:~$ received IRQ and full message: message from peer 10 to peer 11

The ivshmem_send_notify command takes three parameters: the peer ID, the vector and the string to send.