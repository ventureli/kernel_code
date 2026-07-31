# 物理页面状态统计

对应文章：【内核札记-23】物理页面的 N 种状态：看内核怎么给每页内存贴"标签" (2026-07-21)

## 运行结果

```sh
make
sudo insmod ghd.ko
dmesg | tail -30
sudo rmmod ghd
```

## 知识点

- `struct page` — 内核中每页物理内存的描述符
- `page_count()` / `page_mapcount()` — 页面的引用计数
- PG_locked / PG_dirty / PG_writeback 等标志位
- free / allocated / reserved / slab / pagetable 等页面状态分类
