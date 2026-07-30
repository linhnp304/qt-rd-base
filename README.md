# cross_p1 — AR01.01

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

## File cấu hình

`mx01.json` nằm **ngay cạnh file chạy**, sinh ra ở lần chạy đầu. Cả bộ (file
chạy + cấu hình + bản đồ) mang sang máy khác là chạy được ngay.

> Lúc phát triển, file chạy nằm trong `build/` nên cấu hình cũng ở đó và sẽ mất
> khi xoá thư mục `build`. Không sao — thiếu file thì phần mềm dùng giá trị mặc định.

## Nền bản đồ số

Có hai nguồn nền bản đồ, chọn bằng ComboBox trong tab **Cài đặt**:

| Mục trong ComboBox | Nguồn | Dữ liệu |
|---|---|---|
| `MT - …` | Tile ảnh tải sẵn từ MapTiler | `maps/mt/<kiểu-nền>/` |
| `TC` | Lớp vector tự dựng từ shapefile | `maps/tc/` |

Cả hai đều **không nằm trong repo** và đều không bắt buộc: thiếu dữ liệu thì
phần mềm vẫn chạy, chỉ để nền trống kèm dòng nhắc chỉ đúng thư mục còn thiếu.

### Lớp bản đồ TC

Đọc thẳng shapefile (`.shp` + `.dbf` + `.prj`) trong `maps/tc`, không cần thư
viện ngoài: bờ biển, biên giới, địa giới tỉnh, sông ngòi, đường bay dân dụng,
sân bay và tên địa danh. Toạ độ trong tệp có thể đang ở phép chiếu Lambert
Conformal Conic hoặc Transverse Mercator — phần mềm tự đọc `.prj` và quy về
kinh/vĩ độ.

Năm ô ngay dưới ComboBox cho ẩn/hiện từng lớp; lựa chọn được lưu vào `mx01.json`.
Đường dẫn dữ liệu nằm ở trường `vectorDir`, quy tắc giống `tilesDir` bên dưới.

### Tile MapTiler

Máy mới phải tự tải về trước khi nền bản đồ hiện lên.

1. Lấy khoá miễn phí tại <https://cloud.maptiler.com/account/keys/>
2. Lưu vào `docs-local/maptiler.key` (thư mục này đã nằm trong `.gitignore`)
3. Tải tile — mỗi kiểu nền khoảng 70–180 MB:

```bash
python3 tools/download_tiles.py --style basic-v2-dark --min-zoom 11 --max-zoom 14 --radius 50
python3 tools/download_tiles.py --style basic-v2-dark --min-zoom 8  --max-zoom 10 --radius 160
```

Chạy lại không tải trùng, tile đã có sẵn sẽ bỏ qua. Xem `--help` để đổi tâm
hoặc bán kính.

### Kiểu nền bản đồ

Mỗi kiểu nền nằm trong một thư mục con riêng:

```
maps/mt/basic-v2-dark/tileset.json
maps/mt/dataviz-dark/tileset.json
...
```

Phần mềm **tự quét** các thư mục con này để đổ vào ComboBox chọn kiểu nền
(cạnh ô "Hiển thị nền bản đồ"), tên hiện ra có thêm tiền tố `MT - `. Tải thêm
một style là có thêm lựa chọn ngay, không phải sửa code:

```bash
python3 tools/download_tiles.py --style topo-v2-dark --min-zoom 11 --max-zoom 14 --radius 50
```

Tên hiển thị lấy từ trường `label` trong `tileset.json`; đổi bằng `--label`.

### Đổi đường dẫn thư mục bản đồ

Sửa trường `tilesDir` trong `mx01.json` — đây là thư mục **gốc** chứa các kiểu nền:

| Giá trị | Ý nghĩa |
|---|---|
| `maps/mt` | Mặc định — tính từ thư mục chứa file chạy |
| `/media/usb/maps` hoặc `D:/ban-do` | Đường dẫn tuyệt đối, dùng nguyên |

Đường dẫn tương đối còn được dò ngược lên vài cấp thư mục cha, nhờ vậy lúc phát
triển (file chạy trong `build/`) vẫn thấy `maps/mt/tiles` ở gốc repo.

Biến môi trường (tên khai trong [src/appinfo.h](src/appinfo.h), hiện là
`MX01_TILES_DIR`) đè lên tất cả — tiện khi thử nhanh:

```bash
MX01_TILES_DIR=/duong/dan/khac ./cross_p1
```

Bộ mang đi máy khác nên có bố cục:

```
cross_p1                    ← file chạy
mx01.json                   ← cấu hình (tự sinh ở lần chạy đầu)
maps/mt/<kiểu-nền>/         ← tile bản đồ MapTiler, mỗi kiểu một thư mục
maps/tc/                    ← shapefile của lớp bản đồ TC
```

Dữ liệu bản đồ © MapTiler © OpenStreetMap contributors.

## Tách dự án mới từ bộ này

Bộ code này dùng làm gốc cho phần mềm khác có cùng bố cục được. Mọi cái tên đã
gom về **hai chỗ**, sửa xong là chạy — không phải đi tìm tên rải rác trong code:

| Sửa ở đâu | Sửa cái gì |
|---|---|
| [CMakeLists.txt](CMakeLists.txt), dòng `project(...)` | Tên file chạy. **Chữ không dấu**, vì là tên file thật trên đĩa. |
| [src/appinfo.h](src/appinfo.h) | Tên hiển thị, tên đơn vị, tên file cấu hình, biến môi trường. |

Tên hiển thị **viết tiếng Việt có dấu được** — nó chỉ ra tiêu đề cửa sổ và tiêu
đề hộp thoại. Hai thứ này độc lập nhau:

```
project(x123)                                     ← file chạy: x123 / x123.exe
appinfo::displayName() = "Ra đa tầm gần X123"     ← chữ trên thanh tiêu đề
appinfo::configFileName() = "x123.json"           ← file cấu hình, không dấu
```

CI và `tools/download_tiles.py` tự đọc tên từ `project(...)` nên không phải sửa.

Việc còn lại sau khi đổi tên:

1. Sửa dòng tiêu đề và phần mô tả trong README này.
2. `git remote set-url origin <repo mới>` — nhớ làm ngay, kẻo push nhầm về repo gốc.
3. Chép `maps/` sang (không nằm trong repo): tile MapTiler tải lại bằng script,
   riêng `maps/tc` là dữ liệu riêng, phải chép tay.
4. Xoá `build/` cũ nếu có, rồi cấu hình lại từ đầu.

> **MSVC:** mã nguồn là UTF-8 không BOM, nên CMakeLists đã bật sẵn `/utf-8`.
> Bỏ cờ này thì chữ tiếng Việt trên giao diện sẽ thành ký tự lạ khi build bằng
> Visual Studio. GCC và Clang thì mặc định đã đúng.

## CI

GitHub Actions ([.github/workflows/ci.yml](.github/workflows/ci.yml)) build project trên `ubuntu-latest`, `windows-latest`, `macos-latest` ở mỗi push/PR vào `main`.
