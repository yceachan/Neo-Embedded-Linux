# 100ask-support BSP

> [!note]
>
> - [manifests repo](https://gitee.com/weidongshan/manifests) : there u would got the whole BSP environment as the Author`s , supported by 100-ask.
> - [xml url](https://gitee.com/weidongshan/manifests/raw/linux-sdk/imx6ull/100ask_imx6ull_linux4.9.88_release.xml)
>
> - [100ask_imx6ull_linux4.9.88_release.xml](100ask_imx6ull-sdk\.repo\manifests\imx6ull\100ask_imx6ull_linux4.9.88_release.xml) 

#  配置repo

## 下载repo

根据您的办公网络情况，从以下链接获取repo工具。

```
git clone https://gerrit.googlesource.com/git-repo  (谷歌官方源)
git clone https://mirrors.tuna.tsinghua.edu.cn/git/git-repo (国内清华源)
git clone https://gerrit-googlesource.lug.ustc.edu.cn/git-repo (国内中科大源)
```

## 配置REPO_URL

在到构建和谐社会的前提下，需要修改REPO_URL，可参考以下修改：

vim repo

```
## REPO_URL = 'https://gerrit-googlesource.proxy.ustclug.org/git-repo'
REPO_URL = 'https://mirrors.tuna.tsinghua.edu.cn/git/git-repo/'
```

## 配置好的repo(二选一)

```
git clone  https://gitee.com/oschina/repo.git
git clone https://e.coding.net/codebug8/repo.git
```



#  获取视频配套各个开发板bsp源码

## 100ask_imx6ull_pro开发板

```
book@100ask:~$ git clone https://e.coding.net/codebug8/repo.git
book@100ask:~$ mkdir -p 100ask_imx6ull-sdk && cd 100ask_imx6ull-sdk
book@100ask:~/100ask_imx6ull-sdk$ ../repo/repo init -u  https://gitee.com/weidongshan/manifests.git -b linux-sdk -m imx6ull/100ask_imx6ull_linux4.9.88_release.xml  --no-repo-verify
book@100ask:~/100ask_imx6ull-sdk$  ../repo/repo sync -j4
```

## 100ask_imx6ull_pro开发板linux kernel5.4版本

```
book@100ask:~$ git clone https://e.coding.net/codebug8/repo.git
book@100ask:~$ mkdir -p 100ask_imx6ull_pro-sdk && cd 100ask_imx6ull_pro-sdk
book@100ask:~/100ask_imx6ull_pro-sdk$ ../repo/repo init -u  https://gitee.com/weidongshan/manifests.git -b linux-sdk -m imx6ull/100ask_imx6ull_linux5.4.24_release.xml  --no-repo-verify
book@100ask:~/100ask_imx6ull_pro-sdk$  ../repo/repo sync -j4
```



# Addt-manifests-xml


```xml
<?xml version="1.0" encoding="UTF-8"?>

<manifest>
        <remote fetch="https://gitee.com" name="Gitee"/>
        <remote fetch="https://e.coding.net" name="Coding"/>
        <remote fetch="https://e.coding.net/weidongshan/" name="weidongshan"/>

        <default revision="master" remote="Coding" sync-c="true" sync-j="4"/>

        <!-- uboot 2017 -->
        <project name="weidongshan/imx-uboot2017.03" path="Uboot-2017.03" remote="Coding" revision="master"/>

        <!-- uboot 2018.03 -->
        <project name="weidongshan/uboot-imx_2018.03" path="Uboot-2018.03" remote="Coding" revision="master"/> 

        <!-- kernel -->
        <!--<project name="weidongshan/imx-linux4.9.88" path="Linux-4.9.88" revision="master"/>-->
        <project name="weidongshan/imx-linux4.9.88" path="Linux-4.9.88" remote="Coding" revision="master"/>

        <!-- busybox -->
        <project name="weidongshan/busybox" path="Busybox_1.30.0" remote="Gitee"  revision="ef800e5441185585986f9b7aaf39010a926fbd5f"/>

        <!-- Noos project -->
		<project name="weidongshan/imx6ull_NoosProgramProject" path="NoosProgramProject" remote="Gitee"  revision="master"/>

		<!-- buildroot 2020.02.x-->
		<project name="buildroot/buildroot" path="Buildroot_2020.02.x" remote="weidongshan"  revision="master"/>
		<project name="ST-Buildoot-dl/ST-Buildoot-dl" path="Buildroot_2020.02.x/dl"  remote="weidongshan"  revision="imx6ull"/>

        <!-- ToolChain -->
        <project name="weidongshan/ToolChain-6.2.1" path="ToolChain/gcc-linaro-6.2.1-2016.11-x86_64_arm-linux-gnueabihf" revision="master"  remote="Coding" />
		
	    <!-- ToolChain-buildroot2020  -->
		<project name="ToolChain-7.x/ToolChain-7.x" path="ToolChain"  remote="weidongshan"  revision="master"/>

        <!-- DevelopmentEnvConf -->
        <project name="weidongshan/DevelopmentEnvConf" remote="Coding" path="DevelopmentEnvConf" revision="master" />
</manifest>
```

