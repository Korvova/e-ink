"""Send a command to the board over serial and print the log.

Usage:
  python tools/serial_cmd.py COM29 t "Привет, мир!|Вторая строка"   # '|' -> new line
  python tools/serial_cmd.py COM29 i          # network / status JSON
  python tools/serial_cmd.py COM29 p          # toggle current screen
  python tools/serial_cmd.py COM29 --boot     # reset board and show boot log
"""
import sys, time, serial

port = sys.argv[1]
cmd = sys.argv[2] if len(sys.argv) > 2 else 'i'
arg = sys.argv[3] if len(sys.argv) > 3 else ''

s = serial.Serial(port, 115200, timeout=1)
if cmd == '--boot':
    s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False)
    wait = 25
else:
    time.sleep(0.3)
    payload = cmd + arg.replace('|', '\\n') + '\n'
    s.write(payload.encode('utf-8'))
    wait = 45 if cmd == 't' or cmd in '123' else 3

t0 = time.time()
while time.time() - t0 < wait:
    line = s.readline().decode('utf-8', errors='replace').rstrip()
    if line:
        print(f"[{time.time()-t0:5.1f}] {line}")
        if '[job] done' in line and cmd != '--boot':
            break
s.close()
