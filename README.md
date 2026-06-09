# Sliding N_Puzzle

## 1. 安裝必要套件

Ubuntu / Debian：

```bash
sudo apt update

sudo apt install build-essential
sudo apt install libsdl2-dev
sudo apt install libsdl2-image-dev
sudo apt install libsdl2-ttf-dev
```

---

## 2. 編譯

專案目錄：

```text
.
├── sliding_puzzle.c
├── tinyfiledialogs.c
├── tinyfiledialogs.h
└── README.md
```

編譯：

```bash
gcc sliding_puzzle.c tinyfiledialogs.c -o puzzle \
-lSDL2 \
-lSDL2_image \
-lSDL2_ttf
```

執行：

```bash
./puzzle
```

---

## 3. UI 介紹

### 主選單

可選擇拼圖難度：

* 3 × 3
* 4 × 4
* 5 × 5
* 13 × 13

---

### 匯入圖片

點擊 **OPEN IMAGE** 選擇圖片。

支援格式：

* JPG
* PNG

---

### 遊戲畫面

* 滑鼠點擊與空格相鄰的拼圖即可移動
* 右上角顯示遊戲時間
* 下方 **VERIFY** 可檢查是否完成
* **AUTO SOLVE** 可啟動自動解題功能

---

### Auto Solve

解答計算完成後會出現：

* `<`：回到前一步
* `>`：執行下一步
* `AUTO`：自動播放完整解答

!如要切回手動遊玩模式請記得再點擊一次Auto Solve按鈕

---

### 完成畫面

完成拼圖後顯示：

* 解題時間（Solve Time）
* 使用步數（Moves Used）

點擊畫面任意位置即可返回主選單。
