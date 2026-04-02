import serial
import time

def read_serial(comport, baudrate, timeout=1):
    # Configure the serial connection (adjust port and baud rate as needed)
    try:
        ser = serial.Serial(
            port=comport, 
            baudrate=baudrate, 
            timeout=timeout # Timeout prevents the script from blocking forever
        )
        time.sleep(2) # Give the connection time to establish
        print(f"Connected to {comport} at {baudrate} baud")

        while True:
            # Read a line from the serial port (until a newline character \n)
            # The result is a bytes object, so decode it to a string and strip whitespace
            data = ser.readline().decode('utf-8').strip()
            
            if data:
                print(data)

    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
    except KeyboardInterrupt:
        print("Serial read stopped by user.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Serial port closed.")

if __name__ == '__main__':
    # --- Configuration ---
    # On Windows, it's typically 'COMX' (e.g., 'COM3')
    # On Linux/macOS, it's typically '/dev/ttyUSBX' or '/dev/ttyACMX' 
    # (e.g., '/dev/ttyACM0')
    
    SERIAL_PORT ='/dev/cu.usbserial-0001'  # !! CHANGE THIS to your port !!
    BAUD_RATE = 115200      # !! CHANGE THIS to match your device's baud rate !!

    read_serial(SERIAL_PORT, BAUD_RATE)
