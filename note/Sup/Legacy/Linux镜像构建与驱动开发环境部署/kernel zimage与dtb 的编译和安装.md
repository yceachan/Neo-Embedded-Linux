> 准备好内核启动镜像zimage 后，显然将其挂在到uboot的/boot目录下，uboot即可引导内核的加载。

![image-20251225124100968](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/image-20251225124100968.png)

- 编译内核和设备树

  ![image-20251225235946167](C:\Users\yceachan\AppData\Roaming\Typora\typora-user-images\image-20251225235946167.png)

- 编译内核模块

  ![image-20251226000018793](C:\Users\yceachan\AppData\Roaming\Typora\typora-user-images\image-20251226000018793.png)

- 挂载内核、内核模块、设备树：

  ![image-20251226000225108](C:\Users\yceachan\AppData\Roaming\Typora\typora-user-images\image-20251226000225108.png)

- 重启开发板，新的kernel即被替换完成。