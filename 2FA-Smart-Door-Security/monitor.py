import serial
import json
from datetime import datetime

# 埠號請與 Arduino IDE 保持一致 (你剛剛截圖顯示是 COM3)
PORT = 'COM3'
BAUD = 9600

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"=== 智慧門禁資安監控系統已連線 ({PORT}) ===")
    print("請開始刷卡或按鍵盤測試 (按 Ctrl+C 結束並存檔)...\n")

    with open("security_log.csv", "a", encoding="utf-8-sig") as f:
        # 如果是新檔案，先寫入標題列
        if f.tell() == 0:
            f.write("時間,事件類型,卡號UID,詳細原始資料\n")

        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line.startswith("{") and line.endswith("}"):
                now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                data = json.loads(line)
                event = data.get("event", "UNKNOWN")
                uid = data.get("uid", "-")
                
                # 在終端機印出即時紀錄
                print(f"[{now}] 事件: {event:<18} | 卡號: {uid}")
                
                # 同步寫入 CSV 檔案
                f.write(f"{now},{event},{uid},\"{line}\"\n")
                f.flush()

except serial.SerialException:
    print(f"❌ 無法開啟 {PORT}！請確認：\n1. Arduino IDE 的 Serial Monitor 是否已經關掉？\n2. USB 線是否有插好？")
except KeyboardInterrupt:
    print("\n=== 監控已停止，紀錄已儲存至 security_log.csv ===")