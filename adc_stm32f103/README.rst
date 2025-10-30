.. zephyr:code-sample:: adc-stm32f103
   :name: STM32F103 ADC Polling
   :relevant-api: adc_interface

   Periodically read ADC1 channel 0 on Nucleo-F103RB and convert raw counts to millivolts.

Overview
********

This example targets the :zephyr:board:`nucleo_f103rb` board and shows how to
configure the STM32F1 ADC through devicetree. Channel 0 (pin ``PA0``/``A0`` on
Arduino header) is sampled once per second and both the raw 12-bit result and
its millivolt representation are printed to the console.

Key points:

- The devicetree overlay registers ``zephyr,user`` with ``io-channels = <&adc1 0>``.
- Channel configuration (gain, reference, acquisition time, resolution) is done
  through the child node of ``&adc1`` in the overlay.
- The application uses :c:func:`adc_read_dt` and :c:func:`adc_raw_to_millivolts_dt`
  helpers to keep the code portable across boards.

Hardware setup
==============

Connect an analog voltage source (0–3.3 V) to ``PA0`` (Arduino ``A0``) and
share ground with the board. The STM32F103 ADC reference defaults to the
internal 3.3 V (VREF+). Avoid exceeding the input range of the pin.

Building and running
====================

.. zephyr-app-commands::
   :zephyr-app: apps/hello/adc_stm32f103
   :board: nucleo_f103rb
   :goals: build flash
   :compact:

Sample output
=============

.. code-block:: console

   ADC reading[0]:
   - ADC_1, channel 0: 1320 = 1070 mV
   ADC reading[1]:
   - ADC_1, channel 0: 1318 = 1068 mV
   ...

Stop the sample with :kbd:`Ctrl+C` (if running over a terminal monitor) or
reset the board.
