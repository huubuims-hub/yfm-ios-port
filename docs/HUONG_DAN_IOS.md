# Hướng dẫn từng bước: đưa mã nguồn lên GitHub và build iOS bằng GitHub Actions

> Trạng thái: **chưa từng chạy được trên iPhone**. Các lần build đầu có thể báo
> lỗi; đó là bình thường vì mục tiêu là lấy log lỗi để sửa tiếp.

Bạn không cần biết lập trình và không cần dùng dòng lệnh. Mọi thứ làm bằng
chuột với **GitHub Desktop**.

## Bước 1. Tài khoản GitHub

Vào https://github.com, bấm **Sign up**, tạo tài khoản và xác nhận email.

## Bước 2. Cài GitHub Desktop

Vào https://desktop.github.com, tải và cài **GitHub Desktop**, mở lên và đăng nhập.

## Bước 3. Giải nén mã nguồn

1. Giải nén file zip vào thư mục **đường dẫn ngắn**, ví dụ `C:\yfm-ios`.
2. Mở thư mục đó, bạn phải thấy ngay các thư mục `src`, `tools`, `mods`, `docs` và
   `.github`. Nếu thấy thêm một lớp thư mục bọc ngoài, hãy mở vào lớp trong.
3. Đừng bỏ file đĩa game `.bin` vào thư mục này.
4. Bạn **không cần** áp file `.patch`: mã nguồn trong zip đã có sẵn các thay đổi.

## Bước 4. Tạo repo và upload

1. Trong GitHub Desktop: **File > Add local repository...**
2. Bấm **Choose...** và chọn thư mục `C:\yfm-ios`.
3. Khi hiện "This directory does not appear to be a Git repository", bấm liên kết
   **create a repository**, giữ tên mặc định, bấm **Create repository**.
4. Ở ô **Summary** gõ `Initial commit`, bấm **Commit to main**.
5. Bấm **Publish repository**, **bỏ dấu tích "Keep this code private"** (repo phải
   để Public, vì repo private bị tính phút chạy máy Mac nhiều gấp bội), rồi bấm
   Publish.

## Bước 5. Chạy build trên GitHub

1. Vào github.com, mở repo của bạn, chọn tab **Actions**.
2. Chọn **iOS build (experimental)** ở cột trái.
3. Bấm **Run workflow**, ở ô **How far to go** chọn một mức, rồi bấm nút xanh:
   - **deps-only**: dựng thư viện (SDL3, FreeType...) cho iOS.
   - **compile-only**: biên dịch toàn bộ game và gom tất cả lỗi.
   - **full**: liên kết và đóng gói `.ipa`.
4. Tải lại trang và bấm vào dòng đang chạy (tròn vàng). Xanh là thành công, đỏ là lỗi.

## Bước 6. Lấy log và gửi cho mình

1. Kéo xuống cuối trang run, mục **Artifacts**, tải **ios-logs** và giải nén.
2. Mở file **ERRORS-SUMMARY.txt**, chép nội dung (hoặc tải lên cuộc trò chuyện) gửi mình.
3. Nếu không có artifact: bấm vào bước đỏ, kéo xuống cuối và chép khoảng 60 dòng cuối.

## Cập nhật khi có phiên bản mã nguồn mới (làm sau mỗi vòng sửa lỗi)

Mỗi lần mình gửi bản mới (một file zip), bạn **không cần tạo repo lại**:

1. Giải nén file zip mới **đè lên** thư mục cũ (ví dụ `C:\yfm-ios`), chọn
   **Replace the files / Thay thế**.
2. Mở GitHub Desktop. Cột trái hiện các file vừa thay đổi (thường vài chục file).
3. Gõ một dòng mô tả ở ô **Summary** (ví dụ `Update 3`), bấm **Commit to main**.
4. Bấm **Push origin** (nút ở thanh trên).
5. Vào GitHub, tab **Actions**, chạy lại **iOS build (experimental)**.

## Sau khi có file .ipa (chỉ khi chạy `full` thành công)

Tải artifact `yfm-redecomp-ios-arm64-ipa` ở trang run, giải nén ra file `.ipa`, rồi
cài lên iPhone bằng **Sideloadly**, **AltStore** hoặc **SideStore** (Apple ID miễn
phí: app hết hạn sau 7 ngày).

## Đĩa game, mod và gói HD trên iPhone (chưa kiểm chứng)

App chia sẻ thư mục Documents, nên mọi thứ nằm ở **Tệp (Files) > Trên iPhone >
YFM Re-Decomp**. Lần mở đầu app tự tạo thư mục `game` và `mods`.

- **Đĩa game:** chép file `.bin` (USA, SLUS-01411) vào đó, rồi mở lại app.
- **Gói HD:** tải `yfm-redecomp-hd-mod-<phiên bản>.zip` ở trang Releases của dự án
  gốc, chạm vào zip trong Files để giải nén, chuyển thư mục kết quả vào `mods`
  (`mod.json` phải nằm ngay trong thư mục đó).
- Mod code đi kèm (3D Monsters, Hand Camera, AI Hard Mode, Yamyi Mods) nằm sẵn
  trong app. Mod code do bạn tự thêm **không chạy được** trên iOS.

## Nếu app treo vì thiết lập đồ họa quá cao

- Thoát hẳn app (vuốt app lên trong màn hình đa nhiệm) rồi **mở lại**. Nếu lần
  trước app chưa khởi động xong, game sẽ hỏi có đặt lại thiết lập đồ họa không
  (Reset graphics / Reset + turn mods off / Keep settings). File thiết lập cũ được
  giữ là `settings.txt.bak`.
- Nếu hộp thoại không hiện, cách thủ công: mở **Tệp > Trên iPhone > YFM Re-Decomp**,
  **xóa file `settings.txt`** (hoặc mở nó và đặt `internal_scale=1`, `msaa=0`),
  rồi mở lại app.
