# WSL2 搭建NFS Server + 开发板配置NFS Client 完整流程总结

全程基于**WSL2 Ubuntu/Debian**（NFS服务端）+ **嵌入式Linux开发板**（NFS客户端），核心解决WSL2专属坑点（`insecure`参数、IP解析、权限映射），步骤极简可落地，按顺序执行即可实现开发板挂载WSL的NFS共享目录（读写正常）。

## 一、WSL2 端：NFS Server 配置（核心步骤）

### 前提准备

1. 确认WSL为**WSL2**：`wsl -l -v`，若为WSL1执行`wsl --set-version 发行版名 2`升级；
2. 记录WSL2内网IP（开发板挂载用）：`ip a show eth0` → 取`inet`后IP（如`192.168.31.110`）；
3. 确保WSL与开发板**同一内网**（连同一个路由器，IP段一致如`192.168.31.x`）。

### 步骤1：安装NFS服务端依赖

```Bash
sudo apt update && sudo apt install -y nfs-kernel-server rpcbind
```

### 步骤2：创建NFS共享目录（避坑：仅在WSL自身文件系统创建）

**禁止建在****`/mnt/c/d`**（Windows挂载目录，权限失效），建议在`/home/`下创建：

```Bash
# 示例目录：/home/pi/imx/mount，可自定义
sudo mkdir -p /home/pi/imx/mount
# 赋予全权限（双重校验：NFS配置+Linux本地权限）
sudo chmod -R 777 /home/pi/imx/mount
sudo chown -R $(whoami):$(whoami) /home/pi/imx/mount
```

### 步骤3：配置NFS共享规则（/etc/exports，WSL必配`insecure`）

**直接覆盖写入**（避免格式错误，3种客户端规则任选其一，推荐局域网网段）：

```Bash
# 格式：共享目录 客户端IP/网段(核心参数，无空格！)
# 选项1：允许整个局域网（推荐，开发板/虚拟机通用）
sudo echo "/home/pi/imx/mount 192.168.31.0/24(rw,sync,no_root_squash,no_subtree_check,insecure)" > /etc/exports
# 选项2：允许单个开发板IP（最安全，如开发板IP192.168.31.100）
# sudo echo "/home/pi/imx/mount 192.168.31.100(rw,sync,no_root_squash,no_subtree_check,insecure)" > /etc/exports
# 选项3：允许所有设备（测试用，简单）
# sudo echo "/home/pi/imx/mount *(rw,sync,no_root_squash,no_subtree_check,insecure)" > /etc/exports
```

#### 核心参数必选原因（WSL+开发板专属）

- `rw`：读写权限；`sync`：数据同步写入（防丢失）；
- `no_root_squash`：开发板root挂载拥有WSL目录root权限（嵌入式必配，避`Permission denied`）；
- `no_subtree_check`：关闭子目录检查，提升性能；
- `insecure`：WSL2核心坑点，允许非特权端口访问（不加直接挂载失败）。

### 步骤4：加载配置+启动NFS服务（确保规则生效）

```Bash
# 清除旧规则→加载新规则→启动服务→设置开机自启
sudo exportfs -au && sudo exportfs -r
sudo service rpcbind start && sudo service nfs-kernel-server start
sudo update-rc.d rpcbind enable && sudo update-rc.d nfs-kernel-server enable
```

### 步骤5：开放WSL防火墙端口（NFS必备111/2049）

```Bash
# 开放TCP+UDP端口，或直接关闭防火墙（测试用）
sudo ufw allow 111/tcp && sudo ufw allow 111/udp
sudo ufw allow 2049/tcp && sudo ufw allow 2049/udp
sudo ufw reload
# 测试用快捷方式：临时关闭防火墙
# sudo ufw disable
```

### 步骤6：验证WSL NFS服务是否正常（3条命令必过）

```Bash
# 1. 查看生效的共享规则（有输出即正常）
sudo exportfs -v
# 2. 本地查询共享列表（有目录输出即正常）
showmount -e 127.0.0.1
# 3. 检查服务状态（显示active即正常）
sudo service nfs-kernel-server status && sudo service rpcbind status
```

## 二、开发板端：NFS Client 挂载配置（极简步骤）

### 前提准备

1. 开发板联网并与WSL同一内网，记录开发板IP（可选）；
2. 确认开发板已安装NFS客户端工具（嵌入式Linux一般自带，无则手动装）。

### 步骤1：安装NFS客户端依赖（无则执行，如OpenWrt/纯版Linux）

```Bash
# Debian/Ubuntu系开发板
sudo apt install -y nfs-common
# 嵌入式Linux（如RT-Thread/OpenWrt）
opkg install nfs-utils-client
# 龙芯/IMX等原厂系统（一般自带，无需安装）
```

### 步骤2：创建开发板本地挂载点（可自定义，如/mnt/nfs 或 /mnt
）
```Bash
sudo mkdir -p /mnt/nfs  # 推荐单独创建，避免覆盖原有/mnt内容
```

### 步骤3：NFS挂载核心命令（指定NFSv3，嵌入式兼容性最好）

```Bash
# 格式：sudo mount -t nfs -o 客户端参数 WSL2IP:WSL共享目录 开发板挂载点
mount -t nfs -o rw,sync,vers=3,nolock 192.168.31.110:/home/pi/imx/mount /mnt

mount -t nfs -o nolock,vers=3 192.168.31.110:/home/pi/imx/mount /mnt
```

#### 客户端参数说明

- `vers=3`：指定NFSv3版本（WSL的NFSv4偶尔有兼容问题，v3最稳定）；
- `nolock`：嵌入式Linux推荐，关闭文件锁（避免部分开发板锁机制报错）；
- `rw,sync`：与服务端保持一致，强化读写/同步。

### 步骤4：验证挂载是否成功（读写测试+查看挂载状态）

```Bash
# 1. 查看挂载状态（有WSL IP和共享目录即正常）
df -h
# 2. 开发板写入测试（创建文件，WSL端可同步看到即成功）
sudo touch /mnt/nfs/test_from_board.txt
sudo echo "开发板挂载NFS成功" > /mnt/nfs/test_from_board.txt
# 3. WSL端验证（回到WSL执行，能看到文件内容即读写正常）
cat /home/pi/imx/mount/test_from_board.txt
```

### 可选：在`fstab`自动挂载

```Bash
# 编辑fstab，添加挂载规则
sudo vim /etc/fstab
# 写入以下内容（WSL2IP+共享目录+开发板挂载点，按实际修改）
192.168.31.110:/home/pi/imx/mount /mnt/nfs nfs rw,sync,vers=3,nolock 0 0
# 生效配置（无报错即正常）
sudo mount -a
```

⚠️ 注意：若WSL2重启后IP变化，需更新`/etc/fstab`中的WSL IP（WSL2静态IP可自行配置，解决IP变动问题）。

### 开发板卸载NFS目录（如需）

```Bash
# 正常卸载
sudo umount /mnt/nfs
# 强制卸载（挂载卡死时用）
sudo umount -lf /mnt/nfs
```

## 三、核心避坑点（WSL+开发板NFS挂载高频问题）

1. **WSL版本**：必须WSL2，WSL1不支持网络转发和NFS核心功能；
2. **共享目录位置**：禁止在`/mnt/c/d`创建，仅在WSL`/home/`下创建（权限生效）；
3. **参数格式**：`/etc/exports`中参数**无任何空格**，中文括号/逗号会导致解析失败；
4. **IP合法性**：客户端规则不能写`192.168.31.xxx`占位符，用合法格式（网段/具体IP/*）；
5. **双重权限校验**：不仅要NFS配置`rw`，还要给WSL共享目录加`777`Linux本地权限；
6. **WSL IP变动**：WSL2重启后IP会变，需重新记录并更新开发板挂载命令/`fstab`。

## 四、故障快速排查（按顺序查，99%问题可解决）

### 现象1：开发板`showmount -e WSLIP` → 提示`No route to host`

- 排查：WSL与开发板是否同一内网？WSL防火墙是否开放111/2049端口？WSL的rpcbind/nfs-server是否启动？
- 解决：重启路由器/重新连网；WSL执行`sudo ufw disable`（临时关防火墙）；重启NFS服务`sudo service nfs-kernel-server restart`。

### 现象2：开发板挂载 → 提示`Permission denied`

- 排查：WSL的`/etc/exports`是否加`insecure`和`no_root_squash`？共享目录是否`777`权限？客户端是否用`sudo`挂载？
- 解决：重新配置`/etc/exports`并加载；给目录加`chmod 777`；开发板挂载命令加`sudo`。

### 现象3：WSL执行`sudo exportfs -v` → 无输出/解析失败

- 排查：`/etc/exports`格式是否错误？客户端IP/网段是否合法？共享目录是否存在？
- 解决：重新用`echo`覆盖写入合法配置；检查目录是否创建；客户端规则用`192.168.31.0/24`或`*`。

### 现象4：开发板挂载成功但**能读不能写**

- 排查：WSL共享目录Linux本地权限是否不足？`/etc/exports`是否误写`ro`？
- 解决：执行`chmod -R 777 共享目录`；检查`/etc/exports`确保是`rw`而非`ro`，重新加载配置。