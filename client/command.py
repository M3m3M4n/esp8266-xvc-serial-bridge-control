#!/usr/bin/env python3

import argparse
import socket

from consolemenu import *
from consolemenu.items import *
from consolemenu.format import *
from consolemenu.prompt_utils import *

from command_wrapper import CommandWrapper

def handle_exception(menu, server, exception):
    menu.epilogue_text = 'Exception: ' + str(exception)
    if exception is not TimeoutError:
        menu.epilogue_text += '. Try to reconnect to last known IP:Port.'
    else:
        menu.epilogue_text += '.'
    return

def toggle_boot_mode(menu, server):
    print('Sending request...')
    try:
        ret = server.send_command(1)
    except Exception as err:
        handle_exception(menu, server, err)
        return
    ret2 = server.send_command(0, int(not ret[2]))
    if ret2[2] is not ret[2]:
        menu.epilogue_text = 'Boot from ' + ('SD card.' if ret2[2] == 1 else 'NAND.')
    else:
        menu.epilogue_text = 'Exception: State error, before ' + str(ret[2]) + ' after ' + str(ret2[2]) + '.'

def get_boot_mode(menu, server):
    print('Sending request...')
    try:
        ret = server.send_command(1)
    except Exception as err:
        handle_exception(menu, server, err)
        return
    val = int(ret[2])
    if val == 0:
        menu.epilogue_text = 'Board boot mode: NAND.'
    elif val == 1:
        menu.epilogue_text = 'Board boot mode: SD card.'
    else:
        menu.epilogue_text = 'Failure: Unknown value.'
    return

def reset_board(menu, server):
    print('Sending request...')
    try:
        server.send_command(2)
    except Exception as err:
        handle_exception(menu, server, err)
        return
    menu.epilogue_text = 'Board resetting...'
    return

def toggle_xvc_run(menu, server):
    print('Sending request...')
    try:
        ret = server.send_command(4)
    except Exception as err:
        handle_exception(menu, server, err)
        return
    ret2 = server.send_command(3, int(not ret[2]))
    if ret2[2] is not ret[2]:
        menu.epilogue_text = 'XVC server has ' + ('started.' if ret2[2] == 1 else 'stopped.')
    else:
        menu.epilogue_text = 'Exception: State error, before ' + str(ret[2]) + ' after ' + str(ret2[2]) + '.'

def get_xvc_run(menu, server):
    print('Sending request...')
    try:
        ret = server.send_command(4)
    except Exception as err:
        handle_exception(menu, server, err)
        return
    val = int(ret[2])
    if val == 0:
        menu.epilogue_text = 'XVC server is not running.'
    elif val == 1:
        menu.epilogue_text = 'XVC server is running.'
    else:
        menu.epilogue_text = 'Failure: Unknown value.'
    return

def toggle_serial_run(menu, server):
    print('Sending request...')
    try:
        ret = server.send_command(6)
    except Exception as err:
        handle_exception(menu, server, err)
        return
    ret2 = server.send_command(5,  int(not ret[2]))
    if ret2[2] is not ret[2]:
        menu.epilogue_text = 'Serial server has ' + ('started.' if ret2[2] == 1 else 'stopped.')
    else:
        menu.epilogue_text = 'Exception: State error, before ' + str(ret[2]) + ' after ' + str(ret2[2]) + '.'

def get_serial_run(menu, server):
    print('Sending request...')
    try:
        ret = server.send_command(6)
    except Exception as err:
        handle_exception(menu, server, err)
        return
    val = int(ret[2])
    if val == 0:
        menu.epilogue_text = 'Serial server is not running.'
    elif val == 1:
        menu.epilogue_text = 'Serial server is running.'
    else:
        menu.epilogue_text = 'Failure: Unknown value.'
    return

def reconfig_wifi(menu, server):
    screen = Screen()
    if PromptUtils(screen).prompt_for_yes_or_no('This will also reset the command server, continue?'):
        screen.println('Sending request...')
        try:
            server.send_command(7)
        except Exception as err:
            handle_exception(menu, server, err)
            return
        menu.epilogue_text = 'Server reset, connect to AP -> 192.168.4.1.'
    return

def toggle_logger(menu, server):
    screen = Screen()
    screen.println('This will interfere with serial interface as it will inject log in the serial line.')
    screen.println('Also you will need serial server up.')
    if PromptUtils(screen).prompt_for_yes_or_no('Continue?'):
        screen.println('Sending request...')
        try:
            ret = server.send_command(9)
        except Exception as err:
            handle_exception(menu, server, err)
            return
        ret2 = server.send_command(8,  int(not ret[2]))
        if ret2[2] is not ret[2]:
            menu.epilogue_text = 'Serial logging has been ' + ('enabled.' if ret2[2] == 1 else 'disabled.')
        else:
            menu.epilogue_text = 'Exception: State error, before ' + str(ret[2]) + ' after ' + str(ret2[2]) + '.'

def get_logger(menu, server):
    print('Sending request...')
    try:
        ret = server.send_command(9)
    except Exception as err:
        handle_exception(menu, server, err)
        return
    val = int(ret[2])
    if val == 0:
        menu.epilogue_text = 'Serial logging is disabled.'
    elif val == 1:
        menu.epilogue_text = 'Serial server is enabled.'
    else:
        menu.epilogue_text = 'Failure: Unknown value.'
    return

def reset_server(menu, server):
    screen = Screen()
    if PromptUtils(screen).prompt_for_yes_or_no('This will reset the command server, continue?'):
        screen.println('Sending request...')
        try:
            server.send_command(10)
        except Exception as err:
            handle_exception(menu, server, err)
            return
        menu.epilogue_text = 'Server reset, please reconnect.'
    return

def ip_validator(ip):
    try:
        socket.inet_aton(ip)
        return True
    except:
        return False

def port_validator(port):
    try:
        p = int(port)
    except:
        return False
    if p < 0 or p > 65535:
        return False
    return True

def set_new_ip(menu, server):
    old_ip = server.ip
    old_port = server.port
    while True:
        ip = input('New server IP (\"q\" to cancel): ')
        if ip_validator(ip):
            server.ip = ip
            break;
        elif ip == 'q':
            return
        else:
            print('Invalid IP!')
    while True:
        p = input('New server port (\"q\" to cancel): ')
        if port_validator(p):
            server.port = int(p)
            break;
        elif ip == 'q':
            server.ip = old_ip
            return
        else:
            print('Invalid port!')
    menu.epilogue_text = 'New command server ' + str(server.ip) + ':' + str(server.port) + '.'
    menu.subtitle = 'IP: ' + server.ip + ' Port: ' + str(server.port)  + '.'
    return

def reconnect(menu, server):
    try:
        server.connect(server.ip, server.port)
        menu.epilogue_text = 'Connected to ' + str(server.ip) + ':' + str(server.port) + '.'
    except Exception as err:
        handle_exception(menu, server, err)
    return

def functions():
    print('0. Set board boot mode.')
    print('1. Get board boot mode.')
    print('2. Reset board.')
    print('3. Set XVC running state.')
    print('4. Get XVC running state.')
    print('5. Set serial running state.')
    print('6. Get serial running state.')
    print('7. Reconfig wifi.')
    print('8. Set logger state.')
    print('9. Get logger state.')
    print('10. Reset server.')

def loop(server):
    menu_format = MenuFormatBuilder().set_border_style_type(MenuBorderStyleType.HEAVY_BORDER) \
        .set_prompt('SELECT>') \
        .set_title_align('center') \
        .set_subtitle_align('center') \
        .set_left_margin(4) \
        .set_right_margin(4) \
        .show_header_bottom_border(True)
    main_menu = ConsoleMenu('ESP8266 XVC-Serial bridge server control', 'IP: ' + str(server.ip) + ' Port: ' + str(server.port)  + '.' , prologue_text='Check serial bootlog to confirm IP!')

    toggle_boot_mode_menu_func = FunctionItem("Toggle boot mode.", toggle_boot_mode, [main_menu, server])
    main_menu.append_item(toggle_boot_mode_menu_func)

    get_boot_mode_menu_func = FunctionItem("Get board boot mode.", get_boot_mode, [main_menu, server])
    main_menu.append_item(get_boot_mode_menu_func)

    reset_board_menu_func = FunctionItem("Reset board.", reset_board, [main_menu, server])
    main_menu.append_item(reset_board_menu_func)

    toggle_xvc_menu_func = FunctionItem("Toggle XVC server.", toggle_xvc_run, [main_menu, server])
    main_menu.append_item(toggle_xvc_menu_func)

    get_xvc_run_menu_func = FunctionItem("Get XVC server state.", get_xvc_run, [main_menu, server])
    main_menu.append_item(get_xvc_run_menu_func)

    toggle_serial_run_menu_func = FunctionItem("Toggle serial server.", toggle_serial_run, [main_menu, server])
    main_menu.append_item(toggle_serial_run_menu_func)

    get_serial_run_menu_func = FunctionItem("Get serial server state.", get_serial_run, [main_menu, server])
    main_menu.append_item(get_serial_run_menu_func)

    reconfig_wifi_menu_func = FunctionItem("Re-config wifi (Will reset command server).", reconfig_wifi, [main_menu, server])
    main_menu.append_item(reconfig_wifi_menu_func)

    toggle_logger_menu_func = FunctionItem("Toggle serial logger (Need serial server).", toggle_logger, [main_menu, server])
    main_menu.append_item(toggle_logger_menu_func)

    get_logger_menu_func = FunctionItem("Get logger state.", get_logger, [main_menu, server])
    main_menu.append_item(get_logger_menu_func)

    reset_server_menu_func = FunctionItem("Reset command server.", reset_server, [main_menu, server])
    main_menu.append_item(reset_server_menu_func)

    set_new_ip_menu_func = FunctionItem("Set new command server IP.", set_new_ip, [main_menu, server])
    main_menu.append_item(set_new_ip_menu_func)

    reconnect_menu_func = FunctionItem("Connect to command server with known IP.", reconnect, [main_menu, server])
    main_menu.append_item(reconnect_menu_func)

    main_menu.start()
    main_menu.show()

def _main():
    parser = argparse.ArgumentParser(description='Communicate with XVC-Serial bridge server.')
    parser.add_argument('-i','--ip', help='server address.', default=None, required=False)
    parser.add_argument('-p','--port', help='Server command port.', default='42069', required=False, type=int)
    args = parser.parse_args()
    cmdServer = CommandWrapper()
    if(args.ip and args.port):
        try:
            cmdServer.connect(args.ip, args.port)
        except Exception as err:
            print(str(err))
            return
    loop(cmdServer)

if __name__ == '__main__':
    _main()
