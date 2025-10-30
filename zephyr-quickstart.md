# Zephyr 开发环境快速上手：从安装到运行自定义 QEMU 与 ADC 示例

这篇笔记记录了在 Ubuntu 上搭建 Zephyr 开发环境、运行官方 QEMU 示例，并在其基础上实现两个常见场景：
1. 使用软件定时器配合 `k_fifo` 进行批量数据处理。
2. 在 STM32F103 平台上轮询 ADC，读取并转换模拟信号。

内容不仅包含命令步骤，还补充了涉及的内核机制原理，便于理解和二次开发。

## 1. 安装基础依赖

更新系统后安装构建工具链、调试工具及常见依赖：

```bash
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
```

确认关键工具版本满足需求：

```bash
cmake --version
python3 --version
dtc --version
```

准备 QEMU 模拟器（ARM Cortex-M）：

```bash
sudo apt-get install qemu-system-arm
```

## 2. 创建 Python 虚拟环境并安装 West

在工作根目录（示例使用 `~/Workspace/os/zephyrproject`）创建并激活虚拟环境：

```bash
python3 -m venv zephyrproject/.venv
source zephyrproject/.venv/bin/activate
```

安装 Zephyr 官方命令行工具 `west`：

```bash
pip install west
```

## 3. 初始化 Zephyr 工作区

使用 `west` 下载 Zephyr 核心仓库及模块依赖：

```bash
cd zephyrproject
west init .
west update
west zephyr-export
west packages pip --install
```

上述命令会同步官方模块并配置必要的环境变量。

## 4. 安装 Zephyr SDK

在 `zephyr` 子目录中执行 `west sdk install`，按提示选择合适的 SDK 版本和安装路径：

```bash
cd zephyr
west sdk install
```

安装完成后返回工作目录即可继续构建。

## 5. 构建并运行官方 QEMU 示例

使用官方 `samples/synchronization` 验证环境配置：

```bash
west build -b qemu_cortex_m0 samples/synchronization
west build -t run
```

若终端出现两个线程交替打印 “Hello World” 的输出，并能通过 `CTRL+A` `x` 退出 QEMU，即表示基础环境正常。

## 6. 自定义示例：软件定时器批量消费 `k_fifo`

> 目标：通过软件定时器每秒向 `k_fifo` 写入数据，当 FIFO 达到半满时由主线程一次性取出并打印。

1. 复制官方示例作为起点：
   ```bash
   mkdir -p apps/hello
   cp -r zephyr/samples/synchronization apps/hello/kfifo_timer
   ```
2. 修改 `apps/hello/kfifo_timer/prj.conf` 保留最小化配置：
   ```ini
   CONFIG_STDOUT_CONSOLE=y
   CONFIG_PRINTK=y
   CONFIG_MAIN_STACK_SIZE=2048
   ```
3. 使用以下核心代码替换 `src/main.c`，实现软件定时器与 FIFO：
   ```c
   #define FIFO_DEPTH 8
   #define FIFO_HALF (FIFO_DEPTH / 2)

   struct fifo_item {
       void *fifo_reserved;
       uint32_t sequence;
       int64_t uptime_ms;
   };

   K_FIFO_DEFINE(data_fifo);
   K_FIFO_DEFINE(free_pool);
   K_SEM_DEFINE(batch_ready_sem, 0, 1);
   K_TIMER_DEFINE(publisher_timer, publisher_timer_handler, NULL);

   static void publisher_timer_handler(struct k_timer *timer)
   {
       struct fifo_item *item = k_fifo_get(&free_pool, K_NO_WAIT);
       if (item == NULL) {
           printk("producer: no free fifo slots available\n");
           return;
       }

       item->sequence = atomic_add(&sequence_counter, 1) + 1;
       item->uptime_ms = k_uptime_get();

       int previous = atomic_add(&pending_items, 1);
       k_fifo_put(&data_fifo, item);

       if (previous < FIFO_HALF && (previous + 1) >= FIFO_HALF) {
           k_sem_give(&batch_ready_sem);
       }
   }
   ```

4. 主线程启动软件定时器并在收到信号量后批量消费：
   ```c
   void main(void)
   {
       for (int i = 0; i < FIFO_DEPTH; ++i) {
           k_fifo_put(&free_pool, &item_pool[i]);
       }

       k_timer_start(&publisher_timer, K_SECONDS(1), K_SECONDS(1));

       while (1) {
           k_sem_take(&batch_ready_sem, K_FOREVER);
           drain_fifo_batch();
       }
   }
   ```

### 6.1 原理速览：定时器与 FIFO 如何协同

- **软件定时器 (`k_timer`)** 在系统 tick 中断上下文回调，不占用独立线程。适合做轻量级的周期性唤醒或事件投递，本例中只做数据封装和入队以保证实时性。
- **`k_fifo`** 是单生产者/单消费者队列，底层通过链表指针操作完成入队出队。利用 `k_fifo_put()` 与 `k_fifo_get()` 可以在中断和线程之间安全传递消息。
- **对象池 (`free_pool`)** 让 FIFO 元素复用，避免在回调里动态分配内存。Zephyr 推荐使用静态分配手段减轻堆压力和碎片化。
- **原子计数与信号量** 统计当前队列深度，并在达到阈值时通过 `k_sem_give()` 唤醒消费者，从而减少上下文切换次数。

## 7. 构建并运行 `k_fifo` 自定义示例

在工作区根目录下执行：

```bash
west build -b qemu_cortex_m0 apps/hello/kfifo_timer -d build/kfifo_timer
west build -t run -d build/kfifo_timer
```

QEMU 启动后，终端会看到类似输出：

```
k_fifo timer example running on qemu_cortex_m0
consumer: drained 4 item(s) from fifo
consumer: seq=1 uptime=1000 ms
consumer: seq=2 uptime=2000 ms
consumer: seq=3 uptime=3000 ms
consumer: seq=4 uptime=4000 ms
...
```

批量消费完成后，FIFO 会被清空并等待下一轮定时器触发。

## 8. STM32F103 ADC 轮询示例

> 目标：在 Nucleo-F103RB 上访问 `ADC1` 的通道 0，将原始采样值转换为毫伏输出。

1. 基于官方 `adc_dt` 样例复制目录：
   ```bash
   cp -r zephyr/samples/drivers/adc/adc_dt apps/hello/adc_stm32f103
   rm -rf apps/hello/adc_stm32f103/boards apps/hello/adc_stm32f103/socs
   mkdir -p apps/hello/adc_stm32f103/boards
   ```
2. 新增 `boards/nucleo_f103rb.overlay` 绑定 ADC 通道：
   ```dts
   / {
       zephyr,user {
           io-channels = <&adc1 0>;
       };
   };

   &adc1 {
       #address-cells = <1>;
       #size-cells = <0>;

       channel@0 {
           reg = <0>;
           zephyr,gain = "ADC_GAIN_1";
           zephyr,reference = "ADC_REF_INTERNAL";
           zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
           zephyr,resolution = <12>;
       };
   };
   ```
3. 代码层使用 `adc_read_dt()` 与 `adc_raw_to_millivolts_dt()` 读取并换算电压，`prj.conf` 中确保启用 `CONFIG_ADC=y`。
4. 构建与烧录：
   ```bash
   west build -b nucleo_f103rb apps/hello/adc_stm32f103 -d build/adc_stm32f103
   west flash -d build/adc_stm32f103
   ```

### 8.1 原理解析：Zephyr ADC 抽象

- **Devicetree 描述硬件**：`zephyr,user` 节点提供应用层需要的 ADC 通道列表，驱动使用 `ADC_DT_SPEC_GET_BY_IDX` 读取控制器、通道号及增益等参数。
- **采样序列 (`adc_sequence`)**：描述缓冲区、分辨率和触发方式。`adc_sequence_init_dt()` 会根据通道配置填充缺省值，简化应用逻辑。
- **转换流程**：`adc_read_dt()` 调用底层驱动启动一次转换并阻塞等待；随后 `adc_raw_to_millivolts_dt()` 根据参考电压和增益换算具体电压值。
- **STM32F1 特性**：ADC1 通道 0 对应 PA0（Arduino A0），默认参考为 VREF+（3.3V）。如需更精准的参考电压，可外接基准并在 overlay 内调整 `zephyr,reference`。

## 9. 构建验证结果

- QEMU 示例：`west build -b qemu_cortex_m0 apps/hello/kfifo_timer -d build/kfifo_timer`
- STM32F103 示例：`west build -b nucleo_f103rb apps/hello/adc_stm32f103 -d build/adc_stm32f103`

上述命令已在本地执行并通过编译。

## 10. 下一步作业：整合 ADC 与定时批量消费

目标：在 STM32F103 平台上实现一个结合前述两个示例的应用。

- **需求描述**：每 1 秒由软件定时器采集一次 ADC1 通道 0 的电压值；当环形缓冲区（可以继续使用 `k_fifo` 搭配对象池）达到半满时，唤醒主线程批量消费并打印采集到的电压。整体逻辑应模仿 `kfifo_timer` 示例的生产/消费机制，同时沿用 ADC 示例中的硬件依赖与 devicetree 配置。
- **学习重点**：理解 `k_timer`、`k_fifo`、`k_sem`、`adc_read_dt()`、`adc_sequence`、`adc_raw_to_millivolts_dt()` 等 API 的调用时机与运行上下文差异，明确中断回调和线程侧的职责分工。
- **落地建议**：
  1. 在 `apps/hello` 下创建新的工程目录，基础结构可参考 `kfifo_timer` 与 `adc_stm32f103`。
  2. 复用 STM32F103 的 devicetree overlay 和 `prj.conf`，确保 ADC 配置无误。
  3. 定义固定长度的对象池与 `k_fifo`，软件定时器回调中仅完成：从对象池取缓存 → 填充 ADC 结果 → 入队 → 更新计数并在到达半满阈值时投递信号量。
  4. 主线程阻塞等待信号量，批量取出 FIFO 内容后，打印原始值与换算电压，并将对象放回对象池。
  5. 完成后总结实验收获，说明在定时器回调中避免阻塞调用的原因，以及如何保证缓冲区与采样频率匹配。

完成该作业，可帮助建立 Zephyr 中“定时器驱动数据生产 + FIFO/信号量批量消费 + 外设采集”的完整认知。

## 11. 更多资料

- 官方文档：[Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
- API 参考：[k_fifo](https://docs.zephyrproject.org/latest/kernel/services/fifos.html)、[k_timer](https://docs.zephyrproject.org/latest/kernel/services/timers.html)、[ADC API](https://docs.zephyrproject.org/latest/hardware/peripherals/adc.html)
- 工具建议：`tmux new -s zephyr` 可用于分屏构建与查阅文档；`west build -v` 输出详细日志有助于定位问题。
