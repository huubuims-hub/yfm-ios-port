# Hướng dẫn từng bước: đưa mã nguồn lên GitHub và build iOS bằng GitHub Actions

> Trạng thái: **đã chạy được trên iPhone**. Nếu build báo lỗi, hãy gửi log lỗi
> (xem Bước 6) để sửa tiếp.

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

## Icon của app

Icon (chữ "YFM1" và "iOSRecompile") nằm trong `tools/pc/ios_assets/`. Muốn đổi icon,
thay file `AppIcon-1024.png` và các file trong thư mục `icons/` (đúng tên và kích thước),
rồi build lại. Nếu sau khi cài vẫn thấy icon trống, hãy xóa app, cài lại, hoặc khởi động
lại iPhone, vì iOS lưu tạm icon cũ.

## Xem log khi app lỗi hoặc treo (không cần Mac)

App tự ghi log vào thư mục **logs** trong **Tệp > Trên iPhone > YFM Re-Decomp**:

- `log.txt`: log của lần mở app hiện tại.
- `log-previous.txt`: log của lần mở trước. Nếu app vừa bị treo, bị hệ thống tắt
  hoặc bị lỗi, hãy mở lại app rồi gửi file này.
- Thư mục `reports` (cùng cấp): báo cáo khi app bị crash hoặc treo.

Gửi cho mình cả ba nếu có. Trong log có dòng "memory ... MB held, ... MB left,
thermal ..." mỗi 10 giây: nhờ đó biết máy chậm vì nóng (thermal `fair`/`serious`)
hay vì thiếu bộ nhớ ("left" gần 0 trước khi file dừng).

Muốn log chi tiết hơn: tạo file văn bản tên `trace.txt` trong thư mục
YFM Re-Decomp, dòng đầu ghi `frames,window` (hoặc `all`), rồi mở lại app.

## Sao lưu và khôi phục save

Khi xóa app, iOS xóa luôn thư mục Documents của app, kể cả save. Vì vậy:

1. Trong game mở menu **File > Back up saves**. App tạo file
   `backups/yfm-backup-<ngày>-<giờ>.yfmbak` (gồm save, thiết lập và điều khiển).
2. Bấm **Save to Files...**: hộp thoại của iOS hiện ra, chọn **iCloud Drive**,
   **Google Drive** (cần cài sẵn app Google Drive và đã đăng nhập) hoặc một thư
   mục trên máy, rồi bấm Lưu. Bản sao nằm ở đó và vẫn còn khi bạn xóa app.
   (Nếu bấm Done, backup chỉ nằm trong app và sẽ mất khi xóa app.)
3. Sau khi cài lại app: mở game, chọn **File > Restore saves...**, bấm
   **Files...**, chọn file `.yfmbak` đã lưu, rồi xác nhận. Nếu file còn trong
   app thì chọn **In the app** để dùng bản trong thư mục backups.
4. App không tự khởi động lại được: **thoát hẳn app** (vuốt app lên trong màn hình
   đa nhiệm) rồi mở lại. Save được khôi phục lúc app khởi động và có thông báo.
   Đừng lưu game trước khi thoát, vì tiến trình đó sẽ bị bản backup ghi đè.

Trước khi khôi phục, app tự lưu lại save hiện tại thành một backup có đuôi
`-before-restore`, nên có thể hoàn tác bằng cách khôi phục bản đó.
