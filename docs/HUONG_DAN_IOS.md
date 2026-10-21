# Hướng dẫn từng bước: đưa mã nguồn lên GitHub và build iOS bằng GitHub Actions

> Trạng thái: **chưa từng được build trên máy Mac**. Lần chạy đầu rất có thể báo
> lỗi. Đó là bình thường: mục tiêu của các lần chạy đầu là lấy ra log lỗi để sửa tiếp.

Bạn không cần biết lập trình và không cần dùng dòng lệnh. Mọi thứ làm bằng
chuột với **GitHub Desktop**.

## Bước 1. Tài khoản GitHub

1. Vào https://github.com, bấm **Sign up**, tạo tài khoản và xác nhận email.

## Bước 2. Cài GitHub Desktop

1. Vào https://desktop.github.com, tải và cài **GitHub Desktop**.
2. Mở lên, bấm **Sign in to GitHub.com**, đăng nhập tài khoản vừa tạo.

## Bước 3. Giải nén mã nguồn

1. Giải nén file `yfm-ios-port-ready-to-upload.zip` vào một thư mục **đường dẫn ngắn**,
   ví dụ `C:\yfm-ios`.
2. Mở thư mục đó. Bạn phải thấy ngay các thư mục `src`, `tools`, `mods`, `docs`,
   và thư mục `.github`. Nếu thấy thêm một lớp thư mục bọc ngoài, hãy mở vào lớp trong.
3. Đừng bỏ file đĩa game `.bin` vào thư mục này.
4. Bạn **không cần** áp file `.patch`: mã nguồn trong zip này đã có sẵn các thay đổi.

## Bước 4. Tạo repo và upload

1. Trong GitHub Desktop: **File > Add local repository...**
2. Bấm **Choose...** và chọn thư mục `C:\yfm-ios`.
3. Sẽ hiện thông báo "This directory does not appear to be a Git repository".
   Bấm liên kết **create a repository**.
4. Giữ nguyên tên (ví dụ `yfm-ios`), để trống mô tả, bấm **Create repository**.
5. Cột bên trái hiện khoảng 2900 file đã thay đổi. Ở ô **Summary** bên dưới gõ
   `Initial commit`, rồi bấm **Commit to main**.
6. Bấm **Publish repository** (nút xanh ở phía trên).
7. **Bỏ dấu tích ở ô "Keep this code private"** (repo phải để Public, vì repo
   private bị tính phút chạy máy Mac nhiều gấp bội). Bấm **Publish repository**.
8. Chờ upload xong (vài phút).

## Bước 5. Chạy build trên GitHub

1. Vào https://github.com, mở repo `yfm-ios` của bạn, bấm tab **Actions**.
2. Ở cột trái chọn **iOS build (experimental)**.
3. Bấm **Run workflow** (nút bên phải). Ở ô **How far to go** chọn **deps-only**,
   rồi bấm nút xanh **Run workflow**.
4. Tải lại trang, sẽ thấy một dòng có biểu tượng tròn vàng (đang chạy). Bấm vào đó.
   Lần đầu mất khoảng 15 đến 40 phút. Biểu tượng xanh là thành công, đỏ là có lỗi.

## Bước 6. Lấy log và gửi cho mình

1. Bấm vào dòng run đó, rồi bấm vào mục **ios-arm64 (macos-15)** ở cột trái.
2. Bước nào có dấu X đỏ, bấm vào để mở rộng, kéo xuống cuối, **chép khoảng 60 dòng
   cuối** (hoặc chụp màn hình) và gửi cho mình.
3. Kéo xuống cuối trang tóm tắt run, mục **Artifacts**, tải **ios-logs** nếu có.

Nếu `deps-only` xanh, quay lại Bước 5 và chạy tiếp với **compile-only**, rồi
**full**. Khi chạy `compile-only`, mở file `ERRORS-SUMMARY.txt` trong `ios-logs`
và gửi mình nội dung.

## Sau khi có file .ipa (chỉ khi chạy `full` thành công)

Tải artifact `yfm-redecomp-ios-arm64-ipa` ở trang run, giải nén ra file `.ipa`,
rồi cài lên iPhone bằng **Sideloadly**, **AltStore** hoặc **SideStore** (Apple ID
miễn phí: app hết hạn sau 7 ngày).

## Đĩa game, mod và gói HD trên iPhone (chưa kiểm chứng)

App chia sẻ thư mục Documents, nên mọi thứ nằm ở **Tệp (Files) > Trên iPhone >
YFM Re-Decomp**. Lần mở đầu app tự tạo thư mục `game` và `mods`.

- **Đĩa game:** chép file `.bin` (USA, SLUS-01411) vào đó, rồi mở lại app.
- **Gói HD:** tải `yfm-redecomp-hd-mod-<phiên bản>.zip` ở trang Releases của dự án
  gốc, chạm vào zip trong Files để giải nén, chuyển thư mục kết quả vào `mods`
  (`mod.json` phải nằm ngay trong thư mục đó).
- Mod code đi kèm (3D Monsters, Hand Camera, AI Hard Mode, Yamyi Mods) nằm sẵn
  trong app. Mod code do bạn tự thêm **không chạy được** trên iOS.
