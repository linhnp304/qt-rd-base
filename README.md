# cross_p1 — MX01

Màn hình trắc thủ ra đa, Qt Widgets đa nền tảng (Ubuntu, Windows, macOS) — một codebase, build & chạy trên cả ba hệ điều hành.

## Yêu cầu

- CMake >= 3.16
- Qt 6 (Widgets module)
- Trình biên dịch hỗ trợ C++17 (GCC/Clang trên Linux/macOS, MSVC trên Windows)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Chạy binary sinh ra trong thư mục `build/` (ví dụ `build/cross_p1` trên Linux/macOS, `build/Release/cross_p1.exe` trên Windows).

## Nền bản đồ số

Tile bản đồ **không nằm trong repo** (dung lượng lớn, và khoá API là của riêng
từng người). Máy mới phải tự tải về trước khi nền bản đồ hiện lên — thiếu tile
thì phần mềm vẫn chạy bình thường, chỉ để nền đen.

1. Lấy khoá miễn phí tại <https://cloud.maptiler.com/account/keys/>
2. Lưu vào `docs-local/maptiler.key` (thư mục này đã nằm trong `.gitignore`)
3. Tải tile — vùng quanh tâm đài mặc định, khoảng 70 MB:

```bash
python3 tools/download_tiles.py --min-zoom 11 --max-zoom 14 --radius 50
python3 tools/download_tiles.py --min-zoom 8  --max-zoom 10 --radius 160
```

Chạy lại không tải trùng, tile đã có sẵn sẽ bỏ qua. Xem `--help` để đổi style,
tâm hoặc bán kính. Phần mềm tự dò thư mục `docs-local/tiles`; đặt chỗ khác thì
trỏ bằng biến môi trường `MX01_TILES_DIR`.

Dữ liệu bản đồ © MapTiler © OpenStreetMap contributors.

## CI

GitHub Actions ([.github/workflows/ci.yml](.github/workflows/ci.yml)) build project trên `ubuntu-latest`, `windows-latest`, `macos-latest` ở mỗi push/PR vào `main`.
