import sys
from serial import Serial

PORT = 'COM5'

ENQ = '\x05'.encode()
STX = '\x02'.encode()
ETX = '\x03'.encode()
ACK = '\x06'.encode()
EOT = '\x04'.encode()


def send(ser: Serial, msg: bytes):
    ser.write(ENQ)

    ret = ser.read_until(ACK)

    if ret != ACK:
        print("ACK expected, got ", ret.decode())
        return False

    ser.write(STX)
    ser.write(msg)
    ser.write(ETX)

    ret = ser.read_until(ACK)

    if ret != ACK:
        print("ACK expected, got ", ret.decode())
        return False

    ser.write(EOT)

    return True


def receive(ser: Serial) -> str or None:
    ret = ser.read_until(ENQ)

    if ret != ENQ:
        print("ENQ expected, got ", ret.decode())
        return None

    ser.write(ACK)

    ret = ser.read_until(STX)

    if ret != STX:
        print("STX expected, got ", ret.decode())
        return None

    msg = ser.read_until(ETX)

    if not msg.endswith(ETX):
        print("Message does not end with ETX: ", msg.decode())
        return None

    msg = msg[:-1].decode()

    ser.write(ACK)

    ret = ser.read_until(EOT)

    if ret != EOT:
        print("EOT expected, got ", ret.decode())
        return None

    return msg


def main():
    print("Opening serial port: ", PORT)
    ser = Serial(PORT, baudrate=9600, timeout=1)

    motor_is_on = False

    # set motor speed
    # success = send(ser, "#COMMAND:1|MOTOR|SPEED|100".encode())

    # if not success:
    #     print("Unable to set motor speed")
    #     return

    # msg = receive(ser)

    # if msg is None:
    #     return

    while True:
        print("Sending motor ", "on" if motor_is_on else "off")

        if motor_is_on:
            success = send(ser, "#COMMAND:1@MOTOR|ON".encode())

        else:
            success = send(ser, "#COMMAND:1@MOTOR|OFF".encode())

        if success:
            motor_is_on = not motor_is_on

        else:
            break

        msg = receive(ser)

        if msg is not None:
            print(msg)

        else:
            print("Error while receiving")
            break


if __name__ == "__main__":
    try:
        main()

    except KeyboardInterrupt:
        print()
        print("Exiting")
        sys.exit(1)
