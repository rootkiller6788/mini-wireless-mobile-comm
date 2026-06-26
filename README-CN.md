# Mini 无线与移动通信

一套**从零构建、零依赖的 C 语言实现**，涵盖无线通信理论、蜂窝网络与移动系统。每个模块对标斯坦福、MIT 及其他顶尖大学的课程，将教科书公式与 3GPP 标准转化为可运行的 C 代码，架起理论与实践之间的桥梁。

## 子模块

| 子模块 | 主题 | 参考课程 |
|--------|------|----------|
| [mini-5g-nr-phy](mini-5g-nr-phy/) | OFDM 调制、LDPC/Polar 编码、MIMO 预编码、PDCCH 盲检、SSB 小区搜索、DMRS 信道估计 | Stanford EE359, MIT 6.450 |
| [mini-beamforming-massive-mimo](mini-beamforming-massive-mimo/) | 自适应波束成形（LMS/RLS/CMA）、天线阵列设计、DOA 估计（MUSIC/ESPRIT）、MIMO 容量、多用户 MIMO 预编码（MRT/ZF/MMSE） | Stanford EE359, Stanford EE264 |
| [mini-cellular-network](mini-cellular-network/) | 六边形网格建模、频率复用、路径损耗与 SINR、链路预算、功率控制、数据包调度、切换管理 | Stanford EE359, MIT 6.829 |
| [mini-channel-model](mini-channel-model/) | 大/小尺度衰落、多普勒频谱、抽头延迟线多径、MIMO 信道相关性、路径损耗模型 | Stanford EE359, NYU ECE-GY 6013 |
| [mini-handover-mobility](mini-handover-mobility/) | 切换决策算法、协议状态机、信号测量、移动性模型、参数优化 | Stanford EE359, Aalto ELEC-E8004 |
| [mini-lora-nbiot](mini-lora-nbiot/) | LoRa 线性调频扩频（CSS）物理层、LoRaWAN MAC（帧格式/设备类别）、NB-IoT 3GPP 物理层、LPWAN 信道模型 | Stanford EE359, TU Delft ET4386 |
| [mini-wifi-bluetooth](mini-wifi-bluetooth/) | WiFi OFDM 物理层、CSMA/CA MAC、BLE 物理层与 GATT、蓝牙 BR/EDR（FHSS/GFSK）、WPA2/WPA3 安全 | Stanford EE359, MIT 6.829 |
| [mini-wireless-security](mini-wireless-security/) | AES/SHA-256/HMAC 密码学、EAP/802.1X 认证、WEP/WPA2 攻击、密钥管理、物理层安全 | MIT 6.875, Stanford EE359 |

## 设计哲学

- **零外部依赖** — 纯 C（C99/C11），仅依赖 `libc` 和 `libm`
- **模块自包含** — 每个目录拥有独立的 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **标准驱动** — 实现参考 3GPP TS 38.211/38.212/38.213、IEEE 802.11 及 NIST FIPS 标准
- **实用演示** — OFDM 调制器、MIMO 容量仿真器、波束方向图绘制、切换状态机等

## 构建

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-5g-nr-phy
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-wireless-mobile-comm/
├── mini-5g-nr-phy/                 # 5G NR 物理层：OFDM、LDPC/Polar、MIMO、SSB、PDCCH
├── mini-beamforming-massive-mimo/  # 波束成形与大规模 MIMO：自适应、DOA、预编码
├── mini-cellular-network/          # 蜂窝网络：网格、复用、链路预算、调度器
├── mini-channel-model/             # 无线信道：衰落、多普勒、多径、路径损耗
├── mini-handover-mobility/         # 切换与移动性：决策、协议、测量
├── mini-lora-nbiot/                # LoRa 与 NB-IoT：CSS 物理层、LoRaWAN MAC、NB-IoT 物理层
├── mini-wifi-bluetooth/            # WiFi 与蓝牙：OFDM、CSMA/CA、BLE、BR/EDR、WPA
└── mini-wireless-security/         # 无线安全：密码学、认证、攻击、密钥管理
```

## 许可证

MIT
