# deepin-riscv

这是一个将deepin操作系统移植到RISC-V平台的项目，如果有项目需要进行修改我们会在此进行归档，后续会尽量合入主线分支

## 目前适配的硬件平台
基于StarFive JH7100 SoC [VisionFive](https://github.com/starfive-tech/VisionFive/blob/main/README.md) 

## 当前目标
基于debian-ports的环境去适配dde的桌面环境

## 目前进展

可以参考链接中的表格，这是目前已经编译完成的一些项目[**Current-progress**](https://github.com/linuxdeepin/deepin-riscv/blob/master/Current-progress.md)

## 配置riscv64 的构建环境

[riscv64-build-environmen](https://github.com/linuxdeepin/deepin-riscv/blob/master/riscv64-build-environment-zh_CN.md)

## 路线图

- ![avatar](https://github.com/linuxdeepin/deepin-riscv/blob/master/20220222174814.jpg)

## Debian-for-dde-riscv64-img
[Debian-DDE-Beta](https://drive.google.com/file/d/1cboa9MOn6tYSPGa1fnTa3ozG9sTgPi5X/view) 这是一个用于验证的img镜像，目前还有很多问题
非常感谢 [HougeLangley](https://github.com/HougeLangley) 的做相关工作, 这个镜像就是基于 HougeLangley制作的Debian-LXDE镜像修改而来

用户名: star
密码: star
root:root

