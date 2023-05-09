IVSHMEM dual test bed sample app
################################

Prerequisites
*************

* Tested with Zephyr main branch commit id:
   https://github.com/zephyrproject-rtos/zephyr/commit/6697c5aa0bb296ad008ef8f728df254954764962

* Tested agains Zephyr SDK 0.16.1

* QEMU needs to available.

ivshmem-server needs to be available and running. The server is available in
Zephyr SDK or pre-built in some distributions. Otherwise, it is available in
QEMU source tree.

ivshmem-client needs to be available as it is employed in this sample as an
external application. The same conditions of ivshmem-server applies to the
ivshmem-server, as it is also available via QEMU.


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

* Note: Warnings that appear are from ivshmem-shell subsystem and can be ignored.

After getting the both appplications built, open two terminals and run each
instance separately.

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

Long-run usage of this test-bed remarks
***************************************

The command ``ivshmem_send_continuous`` was created with the purpose of create a
stress test environmet of this test bed, its usage is similar of the other commands
in this app.

The long-run test scenario followed the instructions below in the strict sequence
in a clean-state virtual machine running ubuntu 20.04 and 22.04
(Note: Both the tests were executed with 6 hour time duration):

#. Start the ivshmem and set the device owners as described above.
#. Build instance_1 and instance_2 test-bed application in two different terminal tabs.
#. Get, for each instance, their peer id by using ``ivshmem_dump shmem``:

      .. code-block:: console

         uart:~$ ivshmem_dump shmem
         IVshmem up and running:
               Shared memory: 0xafa00000 of size 4194304 bytes
               Peer id: 10
               Notification vectors: 2

#. Select any of instance and issue the ``ivshmem_send_continuous`` command:

      .. code-block:: console

         uart:~$ ivshmem_send_continuous 9 0 "Ping message from ID8"
         Message Ping message from ID8 sent to peer 9 on vector 0

#. Observe the message arrival on the other instance:

      .. code-block:: console

         received IRQ and full message: Ping message from ID8                                                                        "
         received IRQ and full message: Ping message from ID8                                                                        "
         received IRQ and full message: Ping message from ID8                                                                        "
         received IRQ and full message: Ping message from ID8                                                                        "
         received IRQ and full message: Ping message from ID8                                                                        "
         received IRQ and full message: Ping message from ID8                                                                        "
         received IRQ and full message: Ping message from ID8                                                                        "
         received IRQ and full message: Ping message from ID8                                                                        "

#. Let it running for a couple of hours, in this scenario 2 hours, then issue the stop command:

      .. code-block:: console

         uart:~$ ivshmem_send_continuous_stop

#. The message should stop to arrive, observe it on the other instance.
#. Invert the instance, and repeat the last 4 instructions above.
#. Observe the result must be the same, messages arrives without error to the other instances
   and after a stop command it should stop.
#. Perform the ``ivshmem_send_continuous`` command once again and leave it for 6 hours.
#. Observe periodically the other instance, the messages should still arrive contionuosly even after
   hours of running without errors.
#. As a final test step, kill both instances of the test-bed, clean both zephyr workspace and rebuild
   the apps again:

   .. code-block:: console

      $ west build -palways -t run -d instance_1 #-palways will clear the workspace first
      $ west build -palways -t run -d instance_2

#. Re-run the long run tests and observe the results should be the same, message arrivals
   without errors or crashing.

#. Additionally you can create a third instance called instance_3 and also flood the ivshmem
   area:

   .. code-block:: console

      $ mkdir instance_3
      $ west build -palways -bqemu_cortex_a53 -t run -d instance_3

#. Run ``ivshmem_send_continuous`` command from the third instance to intentionally overwrite
   the contents of the other sending instance.

#. Observe the although the buffer received on the receiving instance has corrupt contents
   because there is not locking mechanism on the buffer yet, the application still does not
   crashes and receive the IRQs properly.





