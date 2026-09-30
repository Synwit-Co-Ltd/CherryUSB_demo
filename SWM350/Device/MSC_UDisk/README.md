# SPI NOR vs SPI NAND
## 器件参数
W25Q64JV:   
Sector Erase Time (4KB): 45ms (TYP), 400ms (MAX)    
Page Program Time (256B): 0.8ms (TYP), 3ms (MAX)

W25N01GV:   
Block Erase Time (128KB): 2ms (TYP), 10ms (MAX)     
Page Program Time (2KB): 0.25ms (TYP), 0.7ms (MAX)

## 写入耗时（8MB）
W25Q64JV:   
TYP: 8192 / 4 * 45  + 8192 / 0.25 * 0.8 = 118374ms = 118s   
MAX: 8192 / 4 * 400 + 8192 / 0.25 * 3 = 917504ms = 917s

W25N01GV:   
TYP: 8192 / 128 * 2 + 8192 / 2 * 0.25 = 1152ms = 1s     
MAX: 8192 / 128 * 10 + 8192 / 2 * 0.7 = 3507ms = 3s

## 结论
1. 在大数据量连续写入场景中，NAND Flash 具有显著的时间效率优势。
2. 但使用 NAND Flash 需要软件额外处理 NAND 的坏块管理与 ECC 开销。
