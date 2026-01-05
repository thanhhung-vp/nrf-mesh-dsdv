Bluetooth Mesh DSDV Routing (nRF Connect SDK)
Dự án này triển khai thuật toán định tuyến DSDV (Destination-Sequenced Distance-Vector) trên nền tảng Bluetooth Mesh sử dụng nRF Connect SDK (Zephyr RTOS).

Thay vì sử dụng cơ chế Managed Flooding mặc định của Bluetooth Mesh, dự án này xây dựng một bảng định tuyến (Routing Table) ở tầng ứng dụng (Vendor Model) để xác định đường đi tối ưu đến Sink Node dựa trên số hop và sequence number.

🚀 Tính năng nổi bật
DSDV Algorithm: Tự động cập nhật bảng định tuyến, tránh lặp vòng (loop-free) nhờ số thứ tự (sequence number).

Neighbor Discovery: Tự động phát hiện và đo lường chất lượng tín hiệu (RSSI) của các node hàng xóm.

Mô hình Sink-Node: Hỗ trợ cấu hình thiết bị đóng vai trò là trạm thu (Sink) hoặc cảm biến (Node).

Vendor Model Custom: Sử dụng Model ID 0x1234 để truyền dữ liệu và gói tin định tuyến.

Shell Interface: Tích hợp giao diện dòng lệnh (UART) để debug và kiểm tra bảng định tuyến trực tiếp.

kltn-main/
├── src/
│   ├── main.c              # Khởi tạo Bluetooth Mesh và System
│   ├── chat_cli.c          # Logic chính của DSDV (Xử lý bản tin Hello, Update, Data)
│   ├── model_handler.c     # Khai báo Models và xử lý lệnh Shell
│   ├── neighbor_mgr.c      # Quản lý danh sách hàng xóm
│   └── dsdv_router.c       # Các hàm xử lý thuật toán định tuyến
├── include/                # Các file header (.h)
├── prj.conf                # Cấu hình Kconfig (Bluetooth, Shell, Logging)
└── README.md               # Tài liệu hướng dẫn

🛠 Yêu cầu phần cứng & Phần mềm
Phần cứng:

Tối thiểu 2 Kit phát triển Nordic (nRF52840 DK, nRF5340 DK, hoặc nRF54L15 DK).

Phần mềm:

nRF Connect SDK (v2.6.0 trở lên).

Visual Studio Code + nRF Connect Extension.

Ứng dụng điện thoại nRF Mesh (Android/iOS) để Provisioning.

⚙️ Cấu hình (Sink vs Node)
1. Vai trò của thiết bị được quy định trong code trước khi nạp.

2. Mở file src/chat_cli.c.
Tìm dòng define:
#define IS_SINK_NODE  false

3. Chỉnh sửa:

Để làm Sink Node (Trạm thu): Đổi thành true. (Nạp cho 1 kit).

Để làm Node thường: Đổi thành false. (Nạp cho các kit còn lại).

🔨 Hướng dẫn Build & Flash
1. Build (Biên dịch)
Sử dụng extension nRF Connect hoặc lệnh terminal:
# Build cho nRF52840 DK
west build -b nrf52840dk_nrf52840

# Hoặc build cho nRF54L15 DK (như trong dự án này)
west build -b nrf54l15dk_nrf54l15_cpuapp_ns

2. Flash (Nạp code)
Kết nối kit vào máy tính và chạy:

west flash

📱 Hướng dẫn sử dụng
Bước 1: Provisioning (Cấp phép mạng)
Sau khi nạp code, LED trên kit sẽ nháy báo hiệu chưa vào mạng.

Mở app nRF Mesh trên điện thoại.

Quét và kết nối với thiết bị (tên thường là Zephyr hoặc Chat Node).

Thực hiện Provision.

Vào cấu hình Elements -> Vendor Model (0x1234):

Bind App Key: Chọn App Key 1.

Publish Address: Có thể để mặc định (DSDV sẽ tự xử lý địa chỉ đích trong code).

Bước 2: Sử dụng Shell (Debug)
Mở Terminal (PuTTY / VS Code Serial) với Baudrate 115200.

Các lệnh hỗ trợ:
Lệnh,Mô tả
chat help,Xem danh sách lệnh.
chat routes,"Quan trọng: Xem bảng định tuyến DSDV hiện tại (Dest, Next Hop, Hops)."
chat neighbors,Xem danh sách hàng xóm và RSSI.
chat send <val>,Gửi số <val> về phía Sink Node (dựa trên bảng định tuyến).
chat status,Xem trạng thái node hiện tại.

Ví dụ log: 
uart:~$ chat routes
[DSDV] Routing Table:
Dest    Next    Hops    Seq
0x0001  0x0005  2       12
0x0005  0x0005  1       10

🐞 Troubleshooting
Lỗi BT_MESH_MODEL_HEALTH_SRV khi build:

Đảm bảo đã include đúng header <zephyr/bluetooth/mesh/health_srv.h> trong model_handler.c.

Không thấy Shell:

Kiểm tra dây USB đã cắm vào cổng COM ảo của chip nRF chưa.

Kiểm tra prj.conf đã bật CONFIG_SHELL=y.

Bảng Routing trống:

Đợi khoảng 30s để các node trao đổi bản tin Hello/Update.

Đảm bảo các node nằm trong vùng phủ sóng của nhau.
