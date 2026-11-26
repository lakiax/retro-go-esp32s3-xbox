# Retro-Go ESP32-S3（0.96 寸 / Xbox 手柄版）NES-only 裁剪方案

适用仓库：`lakiax/retro-go-esp32s3-xbox`，master @ `18d1b6c`（2026-01-18）附近的代码结构。
目标：**保留完整的 Retro-Go 系统**（launcher、GUI、设置、书签/浏览器、Wi-Fi/WebUI、0.96 寸屏驱动、I2S 音频、Xbox BLE 手柄、内部 FAT 游戏分区），但**编译进固件、运行时能启动的模拟器只有 nofrendo（NES）**。

配套补丁：`retro-go-nes-only.patch`（已在本仓库代码上验证过改动点，可直接 `git apply`）。

---

## 一、先搞清楚这个项目的架构（否则一定会改错）

Retro-Go 不是"一个固件里链接所有模拟器"，而是**一个 `.img` 里放多个独立的 OTA app 分区**：

```
0x0000   bootloader (ESP32-S3)
0x8000   partition table (0xC00)
0x9000   nvs / otadata / phy_init
0x10000  app: launcher      ← 默认启动，系统 UI
0x110000 app: retro-core    ← 一个分区里塞了 7 个模拟器
0x210000 app: prboom-go
0x310000 app: gwenesis
0x400000 app: fmsx
...      data: vfs (FAT, 放 ROM)
```

- 分区表不是 `partitions.csv`（`rg_tool.py build_app` 写的那个只是给 `idf.py` 用的 dummy），而是 `tools/mkfw.py` 在打包 `.img` 时生成的。
- `rg_tool.py` 的 `PROJECT_APPS` 决定**打包哪些 app 分区、分区顺序和大小**；分区 label 就是 app 目录名。
- launcher 里 `launcher/main/applications.c` 用 `application(描述, short_name, 扩展名, 分区label, crc_offset)` 注册 tab。点 ROM 时调用 `rg_system_switch_app(partition, short_name, path, flags)` → 写 `NS_BOOT/SETTING_BOOT_NAME` → `esp_ota_set_boot_partition(按 label 查找)` → 重启。
- 重启后 `rg_system_init()` 把 `SETTING_BOOT_NAME` 读成 `app->configNs`（见 `rg_system.c:463`），`retro-core/main/main.c` 用 `configNs` 分发到 `gbc_main / nes_main / pce_main / sms_main / gw_main / snes_main / lynx_main`。
- `retro-core/CMakeLists.txt` 的 `COMPONENTS` 白名单把 `nofrendo gnuboy pce-go gw-emulator handy smsplus snes9x` 一起编进这一个 app；`retro-core/main/CMakeLists.txt` 用 `COMPONENT_SRCDIRS "."` 把目录下全部 `main_*.c/.cpp` 一起编进去。

**所以要"只剩 nofrendo"，必须三层同时改，缺一层都不行：**

| 层 | 要改什么 | 不改的后果 |
|---|---|---|
| ① 镜像/打包层 | `rg_tool.py` 的 `PROJECT_APPS` / `DEFAULT_APPS` 只留 `launcher` + `retro-core` | `prboom-go / gwenesis / fmsx` 仍被编译并烧进 `.img` |
| ② 组件/源码层 | `retro-core/CMakeLists.txt` 只列 `nofrendo`，删掉其他 `main_*.c/.cpp`，`main.c` 只保留 `nes` 分支 | 其余 6 个模拟器仍链接进 `retro-core.bin`（体积、符号、运行时分支都在） |
| ③ 启动器 UI 层 | `applications_init()` 只留 `nes` 一条 | 指向 `retro-core` 分区的 SNES/GB/GBC/GW/SMS/GG/COL/PCE/LNX 这些 tab **仍会显示**——`application()` 里的 `rg_system_have_app("retro-core")` 检查的是分区是否存在，分区还在，检查就通过；点进去会因为 `configNs` 无匹配分支而直接退回 launcher |

> 注意：`gba → gbsp`、`doom → prboom-go`、`msx → fmsx`、`md → gwenesis` 这类 tab 在分区被删后会被 `rg_system_have_app()` 自动跳过；**但指向 `retro-core` 的 tab 不会**，这是本方案最关键的坑。

---

## 二、保留 / 删除清单

**保留（一律不要动）**

- `components/retro-go/**`：系统核心，含 `drivers/xbox_driver`（Xbox BLE 手柄）、显示驱动、音频、`rg_storage.c` 的 `vfs` FAT 挂载、`rg_system.c` 的 OTA 切换。
- `components/retro-go/targets/esp32s3-xbox-096inch/{config.h, sdkconfig, env.py}`：161×106 屏、GPIO、`CONFIG_BT_*`、16MB flash、LFN 支持。仅按下文同步 `DEFAULT_APPS`。
- `launcher/**`（除 `main/applications.c` 一处）：GUI、书签、浏览器、WebUI、OTA updater、主题图片。
- `retro-core/main/{main.c, main_nes.c, main_launcher.c, shared.h, CMakeLists.txt}`、`retro-core/components/nofrendo/**`。
- `my_data/roms/nes/`（ROM 目录结构 `my_data/roms/nes/*.nes` 必须保持）。
- `base.cmake`、`tools/mkfw.py`、`tools/gen_images.py`、`themes/`（可选精简）。

**从构建中移除**

- app 目录：`prboom-go/`、`gwenesis/`、`fmsx/`。
- `retro-core/components/` 下：`gnuboy`、`gw-emulator`、`handy`、`pce-go`、`smsplus`、`snes9x`（还有一个只有 CMakeLists 的空壳 `retro-core/components/launcher`，可一并删）。
- `retro-core/main/` 下：`main_gbc.c`、`main_gw.c`、`main_lynx.cpp`、`main_pce.c`、`main_sms.c`、`main_snes.c`。
- `main_lynx.cpp` / `handy` 是 C++，删掉后固件不再需要链接 C++ 运行时，体积进一步下降。

---

## 三、逐文件改动（已包含在补丁里）

### 1. `rg_tool.py`

```diff
-DEFAULT_APPS = os.getenv("RG_TOOL_APPS", "launcher retro-core prboom-go gwenesis fmsx")
+DEFAULT_APPS = os.getenv("RG_TOOL_APPS", "launcher retro-core")
 PROJECT_APPS = {
   'launcher':     [0, 16, 1048576],
   'retro-core':   [0, 16, 1048576],
-  'prboom-go':    [0, 16, 786432],
-  'gwenesis':     [0, 16, 1048576],
-  'fmsx':         [0, 16, 589824],
 }
```

`apps` 参数的 `choices` 由 `PROJECT_APPS.keys()` 生成，改完后传 `prboom-go` 等名字会直接报错，正好防止误构建。分区大小先保持 1MB / 1MB 不动（见"分区偏移"一节）。

### 2. `components/retro-go/targets/esp32s3-xbox-096inch/env.py`

```diff
-# DEFAULT_APPS = "launcher prboom-go"
+DEFAULT_APPS = "launcher retro-core"
```

**必须同步**：`rg_tool.py:187-190` 会 `exec()` 目标目录的 `env.py`，它执行在自己的 `DEFAULT_APPS` 之后，会覆盖前者。两边不一致就会出现"命令行没传 apps 时又去编 5 个 app"的诡异现象。

### 3. `retro-core/CMakeLists.txt`

```diff
-set(COMPONENTS "main retro-go nofrendo gnuboy pce-go gw-emulator handy smsplus snes9x")
+set(COMPONENTS "main retro-go nofrendo")
```

`COMPONENTS` 是 ESP-IDF 的组件白名单；`nofrendo` 的 `COMPONENT_REQUIRES` 只有 `retro-go`，删掉其余组件不会留下悬空依赖。

### 4. `retro-core/main/` —— 删除 6 个入口文件

```bash
git rm retro-core/main/main_gbc.c retro-core/main/main_gw.c \
       retro-core/main/main_lynx.cpp retro-core/main/main_pce.c \
       retro-core/main/main_sms.c retro-core/main/main_snes.c
```

因为该目录用 `COMPONENT_SRCDIRS "."` 通配编译，只改 `COMPONENTS` 不改文件的话，这些 `.c` 仍会被编译并 `#include <gnuboy.h>` 等，直接报错。删完目录只剩 `main.c`、`main_nes.c`、`main_launcher.c`、`shared.h`、`CMakeLists.txt`。

### 5. `retro-core/main/main.c`

```c
void app_main(void)
{
    rg_app_t *app = rg_system_init(AUDIO_SAMPLE_RATE, NULL, NULL);
    RG_LOGI("configNs=%s", app->configNs);

    if (strcmp(app->configNs, "nes") == 0)
        nes_main();
    else
        launcher_main();          // 未知 configNs → 回 launcher，不跑其他模拟器

    RG_PANIC("Never reached");
}
```

`launcher_main()`（`main_launcher.c`）调用 `rg_system_exit()` → `rg_system_switch_app(RG_APP_LAUNCHER, ...)`，是从游戏按 Menu 退回系统的路径，**必须保留**。

### 6. `retro-core/main/shared.h`

只保留 `void launcher_main(); void nes_main();`，删掉其余 `*_main()` 声明和那个从未被定义/使用的 `extern uint8_t shared_memory_block_64K[0x10000]`。

### 7. `launcher/main/applications.c`

`applications_init()` 里只留一行：

```c
application("Nintendo Entertainment System", "nes", "nes fc fds nsf zip", "retro-core", 16);
```

- `part` 必须仍是 `"retro-core"`（分区 label，不要跟着改目录名）。
- `crc_offset = 16` 是 iNES 头长度，保留。
- 末尾 `crc_cache_init()` 保留。

### 8. 文档 / 工程配置

- `cmdlist.txt`：`build-img` 显式写 `launcher retro-core`；`roms.bin` 烧录地址改成从分区表读取的 `<VFS_OFFSET>`。
- `README.md`：去掉失效的 `0x6A0000`，License 段删掉 fMSX / handy 两条例外。
- `BUILDING.md`：更新"retro-core 是合体 app"的说明。
- `retro-go.code-workspace`：移除 `fmsx / gwenesis / prboom-go` 三个 folder。

---

## 四、可选：让源码树也干净（不影响固件，补丁未包含）

只改 `COMPONENTS` 白名单，ESP-IDF 就不会编译这些组件；但源码仍在仓库里。想彻底删：

```bash
git rm -r fmsx gwenesis prboom-go \
  retro-core/components/gnuboy retro-core/components/gw-emulator \
  retro-core/components/handy retro-core/components/pce-go \
  retro-core/components/smsplus retro-core/components/snes9x \
  retro-core/components/launcher
```

同时留意：

- `tools/build_sdl2.sh` 是给 PC 上 SDL2 调试用的，**不是 ESP32 固件构建链路**，删完目录后它会失效；要么同步清理脚本，要么明确它不再可用。
- `retro-core/components/nofrendo/COPYING` 保留；README 的 License 例外说明已同步删除。
- `retro-go.code-workspace` 与 `.github/workflows/ci.yml`（CI 用 Dockerfile + `--target=odroid-go release`，跟本目标无关）按需调整。

### 可选：精简主题图片

`launcher/main/images.c`（约 220KB 文本）由 `tools/gen_images.py` 把 `themes/default/*.png` 全量转成数组。只保留 NES 与通用项可再省几十 KB：

```bash
# 在 themes/default 下，保留：
#   *_nes.png、*_recent.png、*_favorite.png、*_collections.png
# 其余（gb/gbc/gba/gg/gw/lnx/md/msx/ngp/pce/sms/snes/doom/2600/col）删除
python tools/gen_images.py     # 在仓库根目录执行，重写 launcher/main/images.c
```

`gui.c` 的 `gui_get_image()` 按 `类型_名字.png` 查内置图，找不到只是返回 NULL（少一张背景），不会崩溃。这一步纯粹是体积优化，可跳过。

---

## 五、完整构建流程

环境：**ESP-IDF v5.1.6**（fork 验证过的版本；`BUILDING.md` 说 4.4~5.3 通用，但本目标的 sdkconfig 与组件依赖按 5.1 写），已 `export.sh`，`IDF_PATH` 有效。`IDF_TARGET=esp32s3` 由 `env.py` 自动设置，不要手工改成 esp32。

```bash
cd retro-go-esp32s3-xbox
git apply ../retro-go-nes-only.patch        # 或按第三节手工改

# 1) 清一次缓存：CMake 缓存里残留着旧的 COMPONENTS 列表，必须清
python rg_tool.py clean

# 2) 构建完整 .img（两个 app 分区 + 4MB vfs 分区）
python rg_tool.py --target esp32s3-xbox-096inch --fatsize 4194304 \
                  build-img launcher retro-core
# 生成：retro-go_<ver>_esp32s3-xbox-096inch.img
```

说明：

- 不传 apps 参数时走 `DEFAULT_APPS`（已改成 `launcher retro-core`），结果相同；显式写更直观。
- 不要加 `--no-networking`：launcher 的 Wi-Fi / WebUI / OTA updater 属于"完整系统"，保留默认。（Xbox 手柄走 BLE / `CONFIG_BT_*`，与 Wi-Fi 开关无关。）
- 构建日志里的 `Partitions:` 段会打印每个分区的 `size` 和总 `Flash size`，可用来粗略核对。
- 若想压分区：先按 1MB 构建成功，再按 `retro-core/build/retro-core.bin` 实际大小把 `PROJECT_APPS['retro-core'][2]` 调成 `ceil(size/0x10000)*0x10000`。`mkfw.py` 本来就会按实际 bin 大小向上对齐扩容，声明值只是下限，所以**首次构建不要动，最稳**。

### 打包 ROM 镜像

```bash
python %IDF_PATH%/components/fatfs/wl_fatfsgen.py \
       --partition_size 4194304 --output_file roms.bin --long_name_support my_data
```

- 目录结构必须是 `my_data/roms/nes/*.nes`（launcher 的 `paths.roms` 由 `short_name=nes` 拼出）。
- `--partition_size` 必须与 `--fatsize` **完全一致**（4194304），否则挂载后文件系统边界错乱。
- `--long_name_support` 与目标 `sdkconfig` 的 `CONFIG_FATFS_LFN_HEAP=y` 配套；若 ROM 全是 8.3 短名，用不带该参数的命令也可以。

---

## 六、⚠️ 分区偏移必须重算（最容易变砖的一步）

`tools/mkfw.py` 从 `0x10000` 开始按顺序排 app 分区，每个分区实际占用：

```
size_i = ceil( max(声明 size_i, len(app.bin)) / 0x10000 ) * 0x10000
offset(vfs) = 0x10000 + Σ size_i
```

app 从 5 个减到 2 个，`vfs` 分区必然**前移**。README 里的 `0x6A0000`、旧 `cmdlist.txt` 里的 `0x840000` 都是旧布局的值，**直接沿用会把 roms.bin 写进 app 分区，导致无法启动**（重刷 `.img` 可恢复，不算硬砖）。

正确做法——从**本次生成的镜像**读回分区表：

```bash
# 1) 整片刷入新镜像（分区表在 0x8000，必须整片刷，不能只刷某个 app 分区）
python -m esptool -p COM4 -b 921600 write_flash 0x0 \
       .\retro-go_<ver>_esp32s3-xbox-096inch.img

# 2) 读回分区表并解析，找 vfs 那一行的 Offset
python -m esptool -p COM4 read_flash 0x8000 0xC00 partitions.bin
python %IDF_PATH%/components/partition_table/gen_esp32part.py partitions.bin
# 输出示例（数值仅示意）：
#   nvs,      data, nvs,      0x9000,  0x4000,
#   otadata,  data, ota,      0xd000,  0x2000,
#   phy_init, data, phy,      0xf000,  0x1000,
#   launcher, app,  ota_0,    0x10000, 0x100000,
#   retro-core, app, ota_1,  0x110000,0x100000,
#   vfs,      data, fat,      0x210000,0x400000,   ← 用这个 Offset

# 3) 烧录 ROM（把 0x210000 换成实际读到的值）
python -m esptool -p COM4 write_flash <VFS_OFFSET> roms.bin
```

也可以用 `rg_tool.py ... install launcher retro-core` 代替第 1 步（它内部就是 `build-img` + `write_flash 0x0`），但 `roms.bin` 仍必须单独烧。

> 注意：根目录的 `partitions.csv` 是 `build_app()` 写的 dummy（`dummy, app, ota_0, 65536, 3145728`），**不是**真实分区表，别拿它算偏移。

---

## 七、验证清单

1. 构建日志 `Partitions:` 只出现 `launcher`、`retro-core`、`vfs` 三个业务分区（外加 nvs/otadata/phy_init），`Flash size` 明显小于原版。
2. `gen_esp32part.py partitions.bin` 中 app 分区只有两个，label 分别是 `launcher` / `retro-core`，subtype 为 16 / 17；存在 label 为 `vfs` 的 data/fat 分区。
3. ELF 里没有其它模拟器符号：
   ```bash
   xtensa-esp32s3-elf-nm retro-core/build/retro-core.elf | \
     grep -E 'gbc_main|pce_main|sms_main|gw_main|lynx_main|snes_main|snes9x|handy'
   # 应无输出；grep nofrendo / nes_main 应有大量命中
   ```
4. 开机 launcher 只有 **NES** 一个 tab；`<VFS_OFFSET>` 烧录后能看到 `Contra_JP.nes`、`Super Mario Bros.nes`。
5. Xbox 无线手柄可配对（BLE）、按键映射正常；游戏中按 Menu 能退回 launcher；重启后默认回到 launcher。
6. `retro-core/build/retro-core.bin` 体积相对裁剪前显著下降，`.img` 里不再有其它模拟器分区。

---

## 八、常见坑速查

| 现象 | 原因 |
|---|---|
| 只改了 `PROJECT_APPS`，启动器仍显示 SNES/GB/PCE 等 tab | `applications.c` 里那些 tab 指向的 `retro-core` 分区还在，`rg_system_have_app()` 返回 true，必须删注册项 |
| 编译时报 `gnuboy.h: No such file` 之类 | `COMPONENTS` 改了但 `retro-core/main/main_*.c` 没删，`COMPONENT_SRCDIRS "."` 仍通配编译 |
| 命令行没传 apps 时又编出 5 个 app | `env.py` 的 `DEFAULT_APPS` 覆盖了 `rg_tool.py` 的，两处都要改 |
| 点 ROM 后立刻退回 launcher | `configNs` 不是 `"nes"`（一般是 tab 的 `short_name` 与 `main.c` 分支不一致），或 `main.c` 分支被误删 |
| 刷完 `roms.bin` 后设备不开机 | 用了旧的 `0x6A0000` / `0x840000`，写到了 app 分区；重新整片 `write_flash 0x0 <新.img>` |
| FAT 挂载失败 / 看不到 ROM | `--fatsize` 与 `--partition_size` 不一致，或 `my_data/roms/nes/` 目录层级不对 |
| 手柄连不上 | 动了 `components/retro-go/drivers/xbox_driver` 或 `sdkconfig` 里的 `CONFIG_BT_*`；这些必须原样保留 |
| 想把 `retro-core` 改名成 `nofrendo` | 要同步改 4 处：`PROJECT_APPS` 的 key、`applications.c` 的 `part`、`retro-core/CMakeLists.txt` 的 `project()`、目录名；NVS 配置命名空间用的是 `short_name="nes"`，与 app 名无关。**不建议改名**，收益为零、风险不小 |

---

## 九、补丁用法

```bash
cd retro-go-esp32s3-xbox
git apply ../retro-go-nes-only.patch
git status        # 应看到 16 个文件变更（含 6 个 main_*.c/.cpp 删除）
```

补丁**不包含**：物理删除 `fmsx / gwenesis / prboom-go / retro-core/components/*` 大目录、主题 PNG 精简与 `images.c` 重生成、`tools/build_sdl2.sh` 清理——这些是可选的源码树清理，按第四节的命令自行执行。补丁也不包含 `NES_ONLY_BUILD.md` 本身，建议把本文放到仓库根目录一起保留。
