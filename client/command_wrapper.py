import socket
import sys, os

class CommandWrapper:

    HEADER = b'\x04\x20\x69'

    def __init__(self):
        self.conn = None
        self.port = None
        self.ip = None
        return

    # IP is string, port is int
    def __set_and_connect(self, ip, port):
        if not self.__ip_validator(ip) or not self.__port_validator(port):
            raise(Exception('Invalid IP:Port'))
        self.port = port
        self.ip = ip
        try:
            self.conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.conn.connect((ip, port))
            self.conn.settimeout(10)
        except socket.error as err:
            self.conn.close()
            self.conn = None
            raise(err)
        return

    def __recv_respond(self):
        buf = b''
        try:
            for i in range(0,9):
                buf += self.conn.recv(1)
        except socket.timeout as err:
            raise
        if (buf[0:3] != self.HEADER):
            raise(Exception('Received header mismatch: ' + buf[0:3].decode("utf-8")))
        return [buf[3], buf[4], int.from_bytes(buf[5:9], 'little', signed = False)]

    def __ip_validator(self, ip):
        try:
            socket.inet_aton(ip)
            return True
        except:
            return False

    def __port_validator(self, port):
        try:
            p = int(port)
        except:
            return False
        if p < 0 or p > 65535:
            return False
        return True
    '''
    /* CMD Codes
    * 00 set boot mode
    * 01 get boot mode
    * 02 reset
    * 03 set xvc running state disable / enable (set all high z) // also disable xvc loop
    * 04 get xvc running state
    * 05 set serial running state disable / enable (cmd still available) // not sending / reading from serial
    * 06 get serial running state
    * 07 reconfig wifi
    * 08 set logger state
    * 09 get logger state
    * 10 reset server
    */
    '''
    # params and return are byte arrays
    def __prepare_cmd(self, cmd_code_case, extra_data):
        cmd = b''
        if cmd_code_case == b'\x00':
            cmd = self.HEADER + cmd_code_case + extra_data
        elif cmd_code_case == b'\x01':
            cmd = self.HEADER + cmd_code_case
        elif cmd_code_case == b'\x02':
            cmd = self.HEADER + cmd_code_case
        elif cmd_code_case == b'\x03':
            cmd = self.HEADER + cmd_code_case + extra_data
        elif cmd_code_case == b'\x04':
            cmd = self.HEADER + cmd_code_case
        elif cmd_code_case == b'\x05':
            cmd = self.HEADER + cmd_code_case + extra_data
        elif cmd_code_case == b'\x06':
            cmd = self.HEADER + cmd_code_case
        elif cmd_code_case == b'\x07':
            cmd = self.HEADER + cmd_code_case
        elif cmd_code_case == b'\x08':
            cmd = self.HEADER + cmd_code_case + extra_data
        elif cmd_code_case == b'\x09':
            cmd = self.HEADER + cmd_code_case
        elif cmd_code_case == b'\x0a':
            cmd = self.HEADER + cmd_code_case
        return cmd

    def is_connected(self):
        if self.conn is None:
            return False
        try:
            self.conn.send(b'\x00')
        except Exception as err:
            return False
        return True

    def connect(self, ip, port):
        if self.is_connected():
            self.conn.close()
            self.conn = None
        self.__set_and_connect(ip, port)
        return

    # Params as int
    def send_command(self, cmd_code_case, extra_data = 0):
        if not self.is_connected():
            raise Exception('Not connected')
        self.conn.send(self.__prepare_cmd(cmd_code_case.to_bytes(1, byteorder='little'), extra_data.to_bytes(1, byteorder='little')))
        if cmd_code_case == 7 or cmd_code_case == 10:
            return ''
        return self.__recv_respond()

    @staticmethod
    def print_respond_list(respond):
        if respond[0] == 0:
            print('CMD: set boot mode')
        elif respond[0] == 1:
            print('CMD: get boot mode')
        elif respond[0] == 2:
            print('CMD: reset')
        elif respond[0] == 3:
            print('CMD: set xvc running state')
        elif respond[0] == 4:
            print('CMD: get xvc running status')
        elif respond[0] == 5:
            print('CMD: set serial running state')
        elif respond[0] == 6:
            print('CMD: get serial running status')
        elif respond[0] == 7:
            print('CMD: reconfig wifi')
        elif respond[0] == 8:
            print('CMD: set log status')
        elif respond[0] == 9:
            print('CMD: get log status')
        elif respond[0] == 10:
            print('CMD: reset server')
        elif respond[0] == 100:
            print('CMD: test')
        else:
            print('UNKNOWN CMD: transmission error?')
        if respond[1] == 0:
            print('TYPE: status')
        else:
            print('UNKNOWN TYPE: transmission error?')
        print('VALUE: ' + hex(respond[2]))
        return
