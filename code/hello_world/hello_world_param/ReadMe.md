## 如何查看
查看参数

```sh
/sys/module/hello_world_with_param/parameters
# ls -al /sys/module/hello_world_with_param/parameters
total 0
drwxr-xr-x 2 root root    0 2008-01-01 11:27 .
drwxr-xr-x 6 root root    0 2008-01-01 11:27 ..
-rw-rw-r-- 1 root root 4096 2008-01-01 11:27 pid
```


## 读写参数
```sh
# cat pid
0
# echo 1 >>  pid
#
```

## 更新参数以后

```sh
[12505.016714] WQTK: Param pid changed !
[12505.016735] WQTK: Old=0
[12505.016749] WQTK: New=1
```