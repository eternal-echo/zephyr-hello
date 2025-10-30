.. zephyr:code-sample:: kfifo-timer
   :name: Software Timer Batched FIFO
   :relevant-api: fifo_apis timer_apis kernel_api

   Use a Zephyr software timer to feed a k_fifo and consume items in batches.

Overview
********

This application showcases how to combine a :c:type:`k_timer` with
:c:type:`k_fifo` to stage data produced periodically by the system timer.
Every second the timer callback populates an item and pushes it to the FIFO.
Once the queue reaches 50%% of its capacity, the main thread wakes up, drains
all pending items in a single batch, prints their contents, and returns the
structures to a free-list FIFO for reuse.

Building and Running
********************

The sample targets Zephyr's QEMU emulation boards. Build and run it with:

.. zephyr-app-commands::
   :zephyr-app: apps/hello/kfifo_timer
   :host-os: unix
   :board: qemu_cortex_m0
   :goals: run
   :compact:

Sample Output
=============

.. code-block:: console

   k_fifo timer example running on qemu_cortex_m0
   consumer: drained 4 item(s) from fifo
   consumer: seq=1 uptime=1000 ms
   consumer: seq=2 uptime=2000 ms
   consumer: seq=3 uptime=3000 ms
   consumer: seq=4 uptime=4000 ms
   consumer: drained 4 item(s) from fifo
   consumer: seq=5 uptime=5000 ms
   consumer: seq=6 uptime=6000 ms
   consumer: seq=7 uptime=7000 ms
   consumer: seq=8 uptime=8000 ms
   ...

Exit QEMU by pressing :kbd:`CTRL+A` :kbd:`x`.
