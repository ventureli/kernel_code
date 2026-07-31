# Slab 分配器示例

对应文章：【内核札记-26】Slab 分配器：伙伴系统的"精加工车间" (2026-07-25)

演示内核 Slab 分配器的使用：
- `kmem_cache_create()` 创建专用 Slab 缓存
- `kmem_cache_alloc()` 从缓存中分配对象
- `kmem_cache_free()` 归还对象
- 通过 sysfs 动态增减对象数量
- 对比直接使用 kmalloc 的性能优势（对象复用、cache 热度）

## 编译和运行

```bash
make
sudo insmod slabm4.ko
dmesg | tail -10
# 通过 sysfs 操作
echo 10 > /sys/kernel/slab_demo/add_objects
cat /sys/kernel/slab_demo/object_count
sudo rmmod slabm4
```
