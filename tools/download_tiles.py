#!/usr/bin/env python3
"""Tải tile bản đồ MapTiler về đĩa để phần mềm chạy offline.

Khoá API đọc từ docs-local/maptiler.key (thư mục này nằm trong .gitignore).
Mỗi kiểu nền vào một thư mục riêng, tile theo cấu trúc XYZ chuẩn:

    maps/mt/<style>/<z>/<x>/<y>.<png|jpg>
    maps/mt/<style>/tileset.json      <- phần mềm quét file này để lập danh sách

Tải thêm một style là phần mềm tự có thêm lựa chọn trong ComboBox, không cần
sửa code. Chạy lại nhiều lần được: tile đã có sẵn trên đĩa sẽ bị bỏ qua.

Ví dụ:
    python3 tools/download_tiles.py                 # tải theo mặc định
    python3 tools/download_tiles.py --probe         # chỉ thử 1 tile, xem cỡ ảnh
    python3 tools/download_tiles.py --style satellite-v2 --max-zoom 15
"""

import argparse
import concurrent.futures as futures
import json
import math
import os
import sys
import threading
import time
import urllib.error
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KEY_FILE = os.path.join(REPO, "docs-local", "maptiler.key")
# Thư mục gốc chứa bản đồ — trùng với tilesDir trong mx01.json. Mỗi kiểu nền
# nằm trong một thư mục con mang tên style, phần mềm tự quét ra để đổ ComboBox.
BASE_DIR = os.path.join(REPO, "maps", "mt")

# Tên hiển thị trong phần mềm. Style không có trong bảng thì lấy luôn mã style.
LABELS = {
    "basic-v2-dark":   "Tối cơ bản",
    "dataviz-dark":    "Tối giản",
    "streets-v2-dark": "Đường phố",
    "satellite-v2":    "Ảnh vệ tinh",
    "hybrid":          "Vệ tinh + nhãn",
    "topo-v2-dark":    "Địa hình",
    "outdoor-v2-dark": "Dã ngoại",
    "toner-v2":        "Đen trắng",
    "openstreetmap":   "OpenStreetMap",
    "backdrop":        "Nền nhạt",
    "winter-v2":       "Mùa đông",
    "landscape":       "Phong cảnh",
}

# Tâm đài mặc định — trùng với giá trị mặc định trong phần mềm.
DEF_LAT, DEF_LNG = 21.028, 105.852
DEF_RADIUS_KM = 50.0
DEF_MIN_Z, DEF_MAX_Z = 10, 14

USER_AGENT = "MX01-tile-downloader/1.0 (+offline radar display)"


def read_key() -> str:
    if not os.path.exists(KEY_FILE):
        sys.exit(f"Không thấy file khoá: {KEY_FILE}")
    key = open(KEY_FILE, encoding="utf-8").read().strip()
    if not key:
        sys.exit(f"File khoá rỗng: {KEY_FILE}")
    return key


def tile_url(style: str, z: int, x: int, y: int, ext: str, key: str) -> str:
    # Ảnh vệ tinh nằm ở nhánh /tiles/, các style vector-render nằm ở /maps/.
    base = "tiles" if style.startswith("satellite") else "maps"
    return f"https://api.maptiler.com/{base}/{style}/{z}/{x}/{y}.{ext}?key={key}"


def deg2tile(lat: float, lng: float, z: int):
    """lat/lng -> chỉ số tile (có phần lẻ) theo lược đồ XYZ."""
    lat = max(-85.05112878, min(85.05112878, lat))
    n = 2.0 ** z
    xt = (lng + 180.0) / 360.0 * n
    rad = math.radians(lat)
    yt = (1.0 - math.asinh(math.tan(rad)) / math.pi) / 2.0 * n
    return xt, yt


def tile_range(lat, lng, radius_km, z):
    """Khung tile bao trọn hình tròn bán kính radius_km quanh (lat, lng)."""
    dlat = radius_km / 111.32
    dlng = radius_km / (111.32 * math.cos(math.radians(lat)))

    x0, y0 = deg2tile(lat + dlat, lng - dlng, z)   # góc tây bắc
    x1, y1 = deg2tile(lat - dlat, lng + dlng, z)   # góc đông nam

    lo = 0
    hi = 2 ** z - 1
    return (max(lo, int(math.floor(x0))), min(hi, int(math.floor(x1))),
            max(lo, int(math.floor(y0))), min(hi, int(math.floor(y1))))


def fetch(url: str, timeout: int = 30) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def png_size(data: bytes):
    """Đọc kích thước ảnh từ header PNG hoặc JPEG. None nếu không nhận ra."""
    if data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR":
        return int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")
    if data[:2] == b"\xff\xd8":
        i = 2
        while i < len(data) - 9:
            if data[i] != 0xFF:
                i += 1
                continue
            marker = data[i + 1]
            if marker in (0xC0, 0xC1, 0xC2, 0xC3):
                return (int.from_bytes(data[i + 7:i + 9], "big"),
                        int.from_bytes(data[i + 5:i + 7], "big"))
            i += 2 + int.from_bytes(data[i + 2:i + 4], "big")
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--style", default="basic-v2-dark",
                    help="mã style MapTiler (mặc định: basic-v2-dark). "
                         "Gợi ý khác: dataviz-dark, satellite-v2")
    ap.add_argument("--lat", type=float, default=DEF_LAT)
    ap.add_argument("--lng", type=float, default=DEF_LNG)
    ap.add_argument("--radius", type=float, default=DEF_RADIUS_KM, help="km")
    ap.add_argument("--min-zoom", type=int, default=DEF_MIN_Z)
    ap.add_argument("--max-zoom", type=int, default=DEF_MAX_Z)
    ap.add_argument("--out", default=BASE_DIR,
                    help="thư mục gốc chứa bản đồ; tile của mỗi style vào một "
                         "thư mục con mang tên style")
    ap.add_argument("--label", default=None,
                    help="tên hiển thị trong phần mềm (mặc định tra trong bảng)")
    ap.add_argument("--workers", type=int, default=4,
                    help="số luồng tải song song (mặc định 4)")
    ap.add_argument("--delay", type=float, default=0.0,
                    help="nghỉ bao nhiêu giây sau mỗi tile, để đỡ bị chặn tốc độ")
    ap.add_argument("--probe", action="store_true",
                    help="chỉ tải 1 tile để kiểm tra khoá và cỡ ảnh, rồi thoát")
    ap.add_argument("--dry-run", action="store_true",
                    help="chỉ đếm số tile và dung lượng ước tính")
    args = ap.parse_args()

    key = read_key()
    ext = "jpg" if args.style.startswith("satellite") else "png"
    dest = os.path.join(args.out, args.style)   # mỗi style một thư mục riêng

    # --- thử một tile ở giữa vùng, để biết khoá có chạy và tile bao nhiêu px ---
    zc = args.max_zoom
    xc, yc = (int(v) for v in deg2tile(args.lat, args.lng, zc))
    try:
        sample = fetch(tile_url(args.style, zc, xc, yc, ext, key))
    except urllib.error.HTTPError as e:
        body = e.read()[:200].decode("utf-8", "replace")
        return print(f"Lỗi HTTP {e.code} khi thử tile: {body}") or 1
    except Exception as e:                                  # noqa: BLE001
        return print(f"Không gọi được MapTiler: {e}") or 1

    size = png_size(sample)
    if not size:
        return print("Máy chủ trả về dữ liệu không phải ảnh — kiểm tra lại style.") or 1
    tile_px = size[0]
    print(f"Khoá hợp lệ. Style '{args.style}', tile {size[0]}x{size[1]} px, "
          f"định dạng .{ext}")

    if args.probe:
        return 0

    # --- liệt kê công việc ---
    jobs = []
    for z in range(args.min_zoom, args.max_zoom + 1):
        x0, x1, y0, y1 = tile_range(args.lat, args.lng, args.radius, z)
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                jobs.append((z, x, y))
        print(f"  z{z:<3} {x1 - x0 + 1:>4} x {y1 - y0 + 1:<4} = "
              f"{(x1 - x0 + 1) * (y1 - y0 + 1):>6} tile")

    avg_kb = len(sample) / 1024.0
    print(f"\nTổng: {len(jobs)} tile, ước tính "
          f"{len(jobs) * avg_kb / 1024:.0f} MB "
          f"(trung bình {avg_kb:.0f} KB/tile)")

    if args.dry_run:
        return 0

    # --- tải ---
    done = skipped = failed = 0
    lock = threading.Lock()
    t0 = time.time()
    throttled = threading.Event()   # máy chủ đã chặn tốc độ (HTTP 429)
    retry_after = [0]

    def work(job):
        nonlocal done, skipped, failed
        if throttled.is_set():
            return
        z, x, y = job
        path = os.path.join(dest, str(z), str(x), f"{y}.{ext}")
        if os.path.exists(path) and os.path.getsize(path) > 0:
            with lock:
                skipped += 1
            return
        os.makedirs(os.path.dirname(path), exist_ok=True)
        for attempt in range(3):
            try:
                data = fetch(tile_url(args.style, z, x, y, ext, key))
                tmp = path + ".part"
                with open(tmp, "wb") as f:
                    f.write(data)
                os.replace(tmp, path)
                with lock:
                    done += 1
                if args.delay:
                    time.sleep(args.delay)
                return
            except urllib.error.HTTPError as e:
                # 429 = vượt hạn mức. Thử lại chỉ tổ đào sâu thêm, nên dừng cả
                # lượt và báo rõ phải chờ bao lâu.
                if e.code == 429:
                    retry_after[0] = int(e.headers.get("retry-after", 0) or 0)
                    throttled.set()
                    return
                time.sleep(1.5 * (attempt + 1))
            except Exception:                               # noqa: BLE001
                time.sleep(1.5 * (attempt + 1))
        with lock:
            failed += 1

    with futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        for i, _ in enumerate(pool.map(work, jobs), 1):
            if i % 200 == 0 or i == len(jobs):
                print(f"  {i}/{len(jobs)}  tải {done}  bỏ qua {skipped}  "
                      f"lỗi {failed}   ({time.time() - t0:.0f}s)", flush=True)

    if throttled.is_set():
        wait = retry_after[0]
        print(f"\n*** MapTiler đã chặn tốc độ (HTTP 429). Đã tải {done} tile "
              f"trong lượt này. ***")
        if wait:
            print(f"    Phải chờ khoảng {wait // 60} phút ({wait}s) rồi chạy "
                  f"lại lệnh này — tile đã có sẽ được bỏ qua.")
        else:
            print("    Chờ một lúc rồi chạy lại lệnh này — tile đã có sẽ được bỏ qua.")
        print("    Lần sau nên giảm tải: --workers 3 --delay 0.1")
        return 2

    # --- ghi metadata cho phần mềm đọc ---
    # Gộp với lần chạy trước để chạy nhiều lượt (mỗi lượt một dải zoom, bán
    # kính khác nhau) vẫn cho ra dải min/max bao trọn những gì đã có trên đĩa.
    meta_path = os.path.join(dest, "tileset.json")
    old = {}
    if os.path.exists(meta_path):
        try:
            old = json.load(open(meta_path, encoding="utf-8"))
        except (OSError, ValueError):
            old = {}

    # Mỗi lượt tải một dải zoom với bán kính riêng, nên ghi thành danh sách —
    # nói "bán kính 160km" cho cả bộ sẽ sai, vì z11-14 chỉ phủ 50km.
    coverage = [c for c in old.get("coverage", [])
                if c.get("maxZoom", -1) < args.min_zoom
                or c.get("minZoom", 99) > args.max_zoom]
    coverage.append({"minZoom": args.min_zoom, "maxZoom": args.max_zoom,
                     "radiusKm": args.radius})
    coverage.sort(key=lambda c: c["minZoom"])

    meta = {
        "style": args.style,
        "label": args.label or LABELS.get(args.style, args.style),
        "format": ext,
        "tileSize": tile_px,
        "minZoom": min(c["minZoom"] for c in coverage),
        "maxZoom": max(c["maxZoom"] for c in coverage),
        "centerLat": args.lat,
        "centerLng": args.lng,
        "coverage": coverage,
        "attribution": "© MapTiler © OpenStreetMap contributors",
    }
    os.makedirs(dest, exist_ok=True)
    with open(os.path.join(dest, "tileset.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2, ensure_ascii=False)

    print(f"\nXong. Tải mới {done}, có sẵn {skipped}, lỗi {failed}.")
    print(f"Thư mục: {dest}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
