# Hướng dẫn build và cài bản Android (APK)

> Trạng thái: workflow `android-build.yml` và các thay đổi mới (icon, sao lưu
> save, khôi phục thiết lập) **chưa được thử trên máy Android thật**. Nếu build
> hoặc app báo lỗi, hãy gửi log (xem Bước 5).

Bạn không cần máy tính mạnh hay Android Studio: GitHub build giúp, giống cách
build bản iOS.

## Bước 1. Chạy build trên GitHub

1. Vào repo trên GitHub, tab **Actions**.
2. Chọn workflow **Android build (experimental)** ở cột bên trái.
3. Bấm **Run workflow** (nút xanh), rồi **Run workflow** lần nữa.
4. Chờ khoảng 30–90 phút (lần đầu lâu hơn vì phải tải và build thư viện).

## Bước 2. Tải file APK

1. Khi workflow có dấu tích xanh, bấm vào lần chạy đó.
2. Kéo xuống mục **Artifacts**, tải **yfm-android-apk** (file .zip).
3. Giải nén được file `memories-arm64-v8a.apk`.

## Bước 3. Cài vào điện thoại

1. Chép file APK vào điện thoại (cáp USB, Google Drive...).
2. Mở file bằng app Tệp/Files. Android hỏi cho phép "cài đặt ứng dụng không rõ
   nguồn" cho app đang dùng: bật cho app đó, rồi quay lại bấm **Cài đặt**.
3. Lần đầu mở game: chọn file ảnh đĩa (`.bin`/`.iso`) của Forbidden Memories bằng
   hộp thoại hiện ra. Game sẽ chép đĩa vào bộ nhớ của app.

Yêu cầu: điện thoại Android 64-bit (arm64), Android 8 trở lên (API 26).

## Bước 4. Giữ save an toàn khi cập nhật

Android chỉ cho cài **đè** lên bản cũ khi hai bản **cùng chữ ký**. Nếu chữ ký
khác, phải gỡ bản cũ trước, và **gỡ app là mất save**.

- Trước khi gỡ hoặc cài bản mới: trong game mở **File > Back up saves**, bấm
  **Save to Files...** rồi chọn Google Drive hoặc thư mục trên máy. Sau khi cài
  lại: **File > Restore saves... > Files...**, chọn file `.yfmbak`, xác nhận, rồi
  thoát hẳn game và mở lại (xem docs/HUONG_DAN_IOS.md, phần "Sao lưu và khôi
  phục save": Android làm y như vậy).
- Để các lần build sau **cài đè được** mà không cần gỡ, hãy dùng một khóa ký cố
  định (không bắt buộc). Mặc định workflow dùng khóa debug và lưu trong cache của
  GitHub; cache có thể bị xóa sau 7 ngày không dùng, khi đó chữ ký đổi. Khóa cố
  định: tạo một keystore một lần (cần một máy tính có Java, lệnh `keytool`),
  mã hóa base64, rồi thêm 3 secret trong **Settings > Secrets and variables >
  Actions**: `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`,
  `ANDROID_KEY_ALIAS` (mặc định `yfm`). Giữ bản sao của keystore và mật khẩu ở hai
  nơi: mất khóa là không bao giờ cài đè được nữa. Chi tiết: `notes/pc-release.md`,
  phần "Android signing".

## Bước 5. Khi build lỗi hoặc app lỗi

- **Build lỗi:** trong lần chạy đó, tải artifact **android-build-logs**
  (`build.log`, `toolchain.txt`) và gửi mình.
- **App lỗi hoặc treo:** nếu app tự báo "Startup problem" lần mở sau, chọn
  **Reset graphics** để đặt lại đồ họa (file thiết lập cũ được giữ là
  `settings.txt.bak`). Muốn xem log trên máy cần cáp và `adb logcat -s memories`;
  báo cáo crash được lưu trong thư mục của app và game sẽ hỏi chia sẻ ở lần mở sau.

## Build trên máy tính (tùy chọn)

Cần Android SDK (platform 35, build-tools 35, NDK r29), JDK 17, cmake, ninja:

```sh
export ANDROID_SDK_ROOT=/duong/dan/den/sdk
python3 tools/pc/build_game32.py --target android-arm64-v8a --release
# -> tmp/pc/.../memories-arm64-v8a.apk
adb install -r memories-arm64-v8a.apk
```

Chi tiết kỹ thuật: `notes/pc-build.md`, mục "Android".
