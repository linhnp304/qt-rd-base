# cross_p1

Ứng dụng Qt Widgets đa nền tảng (Ubuntu, Windows, macOS) — một codebase, build & chạy trên cả ba hệ điều hành.

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

## CI

GitHub Actions ([.github/workflows/ci.yml](.github/workflows/ci.yml)) build project trên `ubuntu-latest`, `windows-latest`, `macos-latest` ở mỗi push/PR vào `main`.
