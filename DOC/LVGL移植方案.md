# LVGL 8.3.11 移植方案（MYCLASS / STM32F407 + GC9A01 + FreeRTOS）

- 源码位置：`MYCLASS_MX/lvgl-8.3.11`
- 显示：GC9A01 240×240 圆屏，SPI1 1-line 主机（PA5=SCK、PA7=MOSI，PE7=DC、PE9=RES、PE11=CS、PA2=BL）
- 系统：FreeRTOS 10.3.1（CMSIS-RTOS v2），已有一个 `defaultTask`
- 输入：TIM5 编码器（PA0/PA1，驱动 `HARDWARE/encoder.c`）、摇杆 ADC（`HARDWARE/joystick.c`）
- 方案：**LVGL 8.3 + 单刷新任务 + SPI DMA 双部分缓冲 + 编码器 indev**

## 一、总体结构

```
DEMO/应用层        lv_demo_xxx / 自己的界面
        ↓
LVGL 核心          lvgl-8.3.11/src
        ↓
移植层             LVGL_BSP/lv_port_disp.c  (显示)
                  LVGL_BSP/lv_port_indev.c (编码器 / 摇杆)
        ↓
板级驱动           HARDWARE/lcd_spi.c + gc9a01.c（窗口、复位、背光）
                  HARDWARE/encoder.c、joystick.c
```

原则：LVGL 接管屏幕后，**所有屏幕访问只能经过 `lv_port_disp.c`**，不能再在别的任务里直接调 `gc9a01_printf()` 等阻塞刷新函数，否则会和 DMA 刷新抢 SPI1，出现花屏。

## 二、前置改动（必须先做）

| 项 | 当前状态 | 需要改成 | 原因 |
| --- | --- | --- | --- |
| SPI1 TX DMA | 未配置（仅 ADC 用 DMA2_Stream0） | CubeMX 增加 SPI1_TX → **DMA2_Stream3 或 Stream5，Channel3**，Normal、Byte、优先级 5 | 刷新走 DMA，释放 CPU |
| SPI1 速率 | `SPI_BAUDRATEPRESCALER_2` = 42 Mbit/s | 先降到 **/4 = 21 Mbit/s** | 42 Mbit/s 对 GC9A01 偏高，易花屏 |
| FreeRTOS 堆 | `configTOTAL_HEAP_SIZE = 15360` | 改 **32768** | LVGL 任务栈 + 其他任务 |
| LVGL 任务栈 | `defaultTask` 128 words | 新建任务 **1024 words（4KB）** | LVGL 局部变量多 |
| main.c | `osKernelStart()` 前有 `while(1)` 死循环 | 删除 | 否则调度器不启动，LVGL 任务不运行 |

## 三、Keil 工程配置

1. `lv_conf.h` 放在 **`MYCLASS_MX/lv_conf.h`**（即 `lvgl-8.3.11` 的上一级）。LVGL 8.3 默认按 `../../lv_conf.h` 相对路径查找（`src/lv_conf_internal.h`），无需定义 `LV_CONF_INCLUDE_SIMPLE`。
2. 新建组 `LVGL`，按目录添加 `.c`（递归，排除 `tests/`、`docs/`、`env_support/`、`scripts/`）：
   - `src/core`、`src/draw`、`src/font`、`src/hal`、`src/misc`、`src/widgets`、`src/*.c`
   - `src/extra/layouts`、`src/extra/libs`、`src/extra/others`、`src/extra/themes`、`src/extra/widgets`
3. Include 路径加：`MYCLASS_MX`、`MYCLASS_MX/lvgl-8.3.11`、`MYCLASS_MX/HARDWARE`、`MYCLASS_MX/Core/Inc`
4. 编译选项：建议切 **ARM Compiler 6**；若保持 AC5，必须勾选 **C99**。`--C99` 下 LVGL 才能通过编译。
5. 体积控制：`lvgl-8.3.11/demos`、`examples` 默认不加；验证阶段只加 `demos/keypad_encoder`（正好对应编码器输入，且不依赖图片资源）。`demos/widgets` 需要额外的 `examples/assets` 图片，不建议一开始就上。

## 四、lv_conf.h 关键配置

| 宏 | 值 | 说明 |
| --- | --- | --- |
| `LV_COLOR_DEPTH` | `16` | RGB565 |
| `LV_COLOR_16_SWAP` | 先 `0` | 颜色红蓝反了改 `1` |
| `LV_MEM_CUSTOM` | `0` | 使用 LVGL 静态内存池 |
| `LV_MEM_SIZE` | `(24U * 1024U)` | 控件 + 样式 |
| `LV_DISP_DEF_REFR_PERIOD` | `30` | 30 ms 刷新一次 |
| `LV_INDEV_DEF_READ_PERIOD` | `30` | 输入扫描周期 |
| `LV_USE_OS` | `LV_OS_NONE` | 自己开任务，不让 LVGL 再管线程 |
| `LV_USE_LOG` / `LV_USE_ASSERT` | `1` / `2` | 移植期打开，稳定后关 |
| `LV_USE_PERF_MONITOR` | `1` | 调试时看 CPU/帧率 |
| `LV_FONT_MONTSERRAT_14` | `1` | 默认字体 |
| `LV_USE_DEMO_KEYPAD_AND_ENCODER` | `1` | 验证编码器 |

## 五、显示移植（`LVGL_BSP/lv_port_disp.c`）

### 5.1 缓冲

```c
#define LCD_W        240
#define BUF_LINES    40                              /* 240×40×2B = 19.2KB */
static lv_color_t buf1[LCD_W * BUF_LINES];           /* 必须放主 SRAM */
static lv_color_t buf2[LCD_W * BUF_LINES];           /* 不能放 CCM(0x10000000)，DMA 访问不到 */
```

### 5.2 给 lcd_spi 增加 DMA 发送接口

```c
/* lcd_spi.c 新增：DMA 发送，DC=1、CS 保持低，完成后在回调里拉高 CS */
void lcd_spi_write_dma(const uint8_t *buf, uint32_t len)
{
    LCD_DC_DATA();
    LCD_CS_LOW();
    (void)HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *)buf, (uint16_t)len);  /* len < 65535 */
}
```

### 5.3 flush 回调

```c
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    gc9a01_set_window(area->x1, area->y1, area->x2, area->y2);   /* 0x2A/0x2B/0x2C */
    lcd_spi_write_dma((const uint8_t *)color_p,
                      (uint32_t)(area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * 2U);
    /* 不在这里调用 lv_disp_flush_ready()，等 DMA 完成 */
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        lcd_spi_cs_high();                 /* 结束本次传输 */
        lv_disp_flush_ready(&disp_drv);
    }
}
```

初始化顺序：`gc9a01_init()` → `lcd_spi_init()`（复位、背光）→ `lv_port_disp_init()`（注册 `lv_disp_drv_t`、设置 `hor_res/ver_res = 240`、绑定两块缓冲、调用 `lv_disp_drv_register`）。

注意：
- 注册两块缓冲后 LVGL 会自动交替使用；每次 flush 结束调用且只调用一次 `lv_disp_flush_ready()`，不要重复调用。
- DMA 传输长度上限 65535 字节；`BUF_LINES=40` 时单次 19200 字节，安全。
- `HAL_SPI_TxCpltCallback` 全工程只能有一个；ADC 用 DMA2_Stream0，不同流，互不影响。

## 六、tick 与刷新任务

```c
static void lvgl_task(void *arg)
{
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    lv_demo_keypad_encoder();      /* 或自己的界面 */

    for (;;) {
        lv_tick_inc(5);            /* 5 ms 精度，UI 动画足够 */
        lv_timer_handler();
        osDelay(5);
    }
}
```

- 任务优先级 `osPriorityNormal`，栈 `1024 * 4` 字节（CMSIS-RTOS v2 的 stack_size 单位为字节）。
- 不需要 `LV_TICK_CUSTOM`：`configUSE_TICK_HOOK` 当前为 0，且 `FreeRTOSConfig.h` 是生成文件，改 tick hook 会在重新生成时丢失。
- 所有 `lv_*` 调用集中在 `lvgl_task` 中；中断里只允许调用 `lv_disp_flush_ready()`。

## 七、输入设备（`LVGL_BSP/lv_port_indev.c`）

### 7.1 编码器（焦点导航）

```c
static void enc_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    data->enc_diff = (int16_t)encoder_delta();
    data->state    = LV_INDEV_STATE_RELEASED;   /* 有确认按键时按实际电平改 */
}
```

- 注册类型 `LV_INDEV_TYPE_ENCODER`，采样周期 30 ms。
- **必须创建 group 并绑定**，否则编码器只改值、不会切换焦点：

```c
lv_group_t *g = lv_group_create();
lv_indev_set_group(enc_indev, g);   /* enc_indev 为 lv_indev_drv_register() 的返回值 */
lv_group_set_default(g);   /* 后续新建控件自动入组 */
```

- 确认键：编码器模块自带的按键（若有）接一个 GPIO，`state` 返回 `LV_INDEV_STATE_PRESSED` 时 LVGL 视为 `LV_KEY_ENTER`。
- 方向与预期相反时，改 `encoder.h` 的 `ENCODER_DIR_INVERT` 为 1，不要动 CubeMX 极性。

### 7.2 摇杆（方向键）

注册 `LV_INDEV_TYPE_KEYPAD`，`read_cb` 里用 `joy_get_dir()` 把 8 方向映射到 `LV_KEY_LEFT/RIGHT/UP/DOWN`（斜向同时给两个键），按摇杆按下键给 `LV_KEY_ENTER`。适合列表、菜单类界面。

## 八、内存预算（SRAM 192KB）

| 项目 | 占用 |
| --- | --- |
| LVGL 静态池 `LV_MEM_SIZE` | 24KB |
| 显示缓冲 2×19.2KB | 38.4KB |
| FreeRTOS 堆（任务栈等） | 32KB |
| 现有驱动/全局变量 | ~15KB |
| 合计 | ~110KB，余量充足 |

Flash：LVGL 8.3 核心 + keypad demo 约 120~200KB，F407VG 1MB 足够。字体/图片数组务必加 `const`，放 Flash 而不是 RAM。

## 九、分阶段验收

| 阶段 | 内容 | 验收标准 |
| --- | --- | --- |
| 1 | 加源码、改 lv_conf、工程能编译 | 0 error，链接成功 |
| 2 | 阻塞版 flush（不 DMA），RTOS 任务里跑 keypad demo | 画面正常、颜色正确、坐标方向对 |
| 3 | 换 DMA + 双缓冲 | 满屏刷新 < 50 ms，无花屏/闪烁 |
| 4 | 编码器 indev + group | 转动编码器焦点在控件间移动，按下确认 |
| 5 | 摇杆 keypad | 推摇杆能滚动列表 |
| 6 | 中文字体 | 指定汉字正常显示 |

阶段 2 若出现花屏，先把 SPI 降到 10.5 Mbit/s（分频 /8）确认是时序问题还是 DMA 问题。

## 十、常见问题排查

| 现象 | 可能原因 | 处理 |
| --- | --- | --- |
| 编译报 `lv_conf.h` 找不到 | 文件位置不对 | 放到 `MYCLASS_MX/lv_conf.h`，或定义 `LV_CONF_INCLUDE_SIMPLE` 并加 include 路径 |
| 全屏花屏/撕裂 | SPI 过快、CS/DC 时序、DMA 与阻塞 SPI 混用 | 降 SPI 速率；确认没有其他任务调 `gc9a01_printf` |
| 颜色红蓝互换 | 像素字节序 | `LV_COLOR_16_SWAP 1` |
| 图像上下/左右反 | MADCTL 扫描方向 | 改 GC9A01 `0x36` 参数或 `lv_disp_set_rotation` |
| 界面卡死、不出图 | 没有循环调用 `lv_timer_handler()`，或 tick 不增长 | 检查任务是否运行、`lv_tick_inc` 是否调用 |
| 编码器无反应 | 未创建/绑定 group；TIM5 未启动 | `encoder_init()` 是否调用；`lv_group` 绑定 |
| 内存分配失败打印 | `LV_MEM_SIZE` 太小 | 先加到 32KB 定位，再按实际回调 |
| HardFault | 缓冲放在 CCM；栈溢出 | DMA 缓冲移到主 SRAM；调大任务栈并开栈检查 |

## 十一、新增/修改文件清单

| 文件 | 动作 |
| --- | --- |
| `MYCLASS_MX/lv_conf.h` | 由 `lv_conf_template.h` 复制并改配置 |
| `LVGL_BSP/lv_port_disp.c/.h` | 新增：显示注册、双缓冲、DMA flush |
| `LVGL_BSP/lv_port_indev.c/.h` | 新增：编码器 / 摇杆输入 |
| `HARDWARE/lcd_spi.c/.h` | 修改：新增 `lcd_spi_write_dma()`、`lcd_spi_cs_high()` |
| `Core/Src/freertos.c` | 修改：新建 `lvgl_task`（USER CODE 区） |
| `Core/Inc/FreeRTOSConfig.h` | 修改：堆 15KB → 32KB |
| `Core/Src/main.c` | 修改：删除 `osKernelStart()` 前的 `while(1)` 调试循环 |
| `MYCLASS_MX.ioc` | 修改：SPI1 增加 TX DMA、降分频 |
| Keil 工程 | 新增 `LVGL` 组、include 路径、C99/AC6 |
