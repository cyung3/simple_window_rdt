#!/usr/bin/env python3

"""
Author: Zhiyi Zhang (zhiyi@cs.ucla.edu), Qianru Li (qianruli@cs.ucla.edu)
For the purpose of CS118 2020 Spring Project 2 output result check
Usage: ./test.py output_file or python3 test.py output_file
"""

import sys
import re

all_lines = None
output_pattern = re.compile(
    r'(?P<event>\w+) (?P<seq>\d+)\s?(?P<ack>\d+)?\s?(?P<syn_flag>SYN)?\s?(?P<fin_flag>FIN)?\s?(?P<ack_flag>ACK)?\s?(?P<dup_ack_flag>DUP-ACK)?')

my_isn = None
server_isn = None
my_fin = None
server_fin = None


def get_filename() -> str:
    if len(sys.argv) < 2:
        print(
            "Usage:", sys.argv[0], "output_file_name \n", file=sys.stderr)
        exit(-1)
    return sys.argv[1]

def check_timeout(all_lines) -> bool:
    is_timeout = False
    resend_counter = 0
    step = 512
    timeout_seq = 0
    is_syn_timeout = False
    is_syn_third_pkt_timeout = False
    is_fin_timeout = False
    is_handshake = False
    is_teardown = False

    pass_check = True

    global my_isn, server_isn, my_fin, server_fin
    for lino, line in enumerate(all_lines):
        if len(line) == 0:
            continue
        result = output_pattern.match(line)
        result = result.groupdict()

        # SEND SYN, get ISN
        if result['event'] == 'SEND' and result['syn_flag'] is not None:
            my_isn = int(result['seq'])
            print('Client ISN: ', my_isn)
            is_handshake = True

        # RECV SYN ACK, get ISN
        if result['event'] == 'RECV' and result['syn_flag'] is not None and result['ack_flag'] is not None:
            server_isn = int(result['seq'])
            print('Server ISN: ', server_isn)

        # SEND FIN
        if result['event'] == 'SEND' and result['fin_flag'] is not None:
            my_fin = int(result['seq'])

        # RECV FIN
        if result['event'] == 'RECV' and result['fin_flag'] is not None:
            server_fin = int(result['seq'])

        # Record TIMEOUT
        if result['event'] == 'TIMEOUT':
            if int(result['seq']) == my_isn:
                is_syn_timeout = True
            elif int(result['seq']) == (my_isn + 1) % 25601:
                is_syn_third_pkt_timeout = True
            elif int(result['seq']) == my_fin:
                is_fin_timeout = True
            else:
                is_timeout = True
                timeout_seq = int(result['seq'])
                resend_counter = 0
            continue

        if is_syn_timeout:
            if result['event'] == 'RECV' and result['syn_flag'] is not None:
                pass
            elif result['event'] == 'SEND':
                print('[Error] Send data after SYN loss: line', lino+1, line)
                pass_check = False
                # return False
            elif result['event'] == 'RESEND' and int(result['seq']) != my_isn:
                print('[Error] Wrong seq in resent SYN: line', lino+1, line)
                pass_check = False
                # return False
            elif result['event'] == 'RESEND' and int(result['seq']) == my_isn:
                is_syn_timeout = False
        elif is_syn_third_pkt_timeout:
            if result['event'] == 'RECV':
                pass
            elif result['event'] == 'RESEND' and int(result['seq']) != (my_isn + 1) % 25601:
                print('[Error] Wrong seq in resent third-way SYN-ACK: line', lino+1,line)
                pass_check = False
                # return False
            elif result['event'] == 'RESEND' and int(result['seq']) == (my_isn + 1) % 25601:
                is_syn_third_pkt_timeout = False
        elif is_fin_timeout:
            if result['event'] == 'RECV':
                pass
            elif result['event'] == 'RESEND' and (int(result['seq']) != my_fin or result['fin_flag'] is None):
                print('[Error] Resend FIN: line', lino+1, line)
                pass_check = False
                # return False
            elif result['event'] == 'RESEND' and int(result['seq']) == my_fin and result['fin_flag'] is not None:
                is_fin_timeout = False
        elif is_timeout and resend_counter < 10:
            if result['event'] == 'RECV':
                continue
            elif result['event'] == 'RESEND' and int(result['seq']) != (timeout_seq + step * resend_counter) % 25601:
                # print('bad resend seq: %d, %d'%(int(result['seq']), (timeout_seq + step * resend_counter) % 25601))
                print('[Error] Wrong seq in resent data: line', lino+1, line)
                pass_check = False
                # return False
            elif result['event'] == 'RESEND' and int(result['seq']) == (timeout_seq + step * resend_counter) % 25601:
                resend_counter += 1
                if resend_counter is 10:
                    is_timeout = False
                    resend_counter = 0

    if is_syn_timeout:
        print('[Error] SYN timeout not solved.')
        pass_check = False
        # return False
    if is_syn_third_pkt_timeout:
        print('[Error] Third-way SYN-ACK timeout not solved.')
        pass_check = False
        # return False
    if is_fin_timeout:
        print('[Error] FIN timeout not solved.')
        pass_check = False
        # return False
    if is_timeout:
        print('[Warning] DATA timeout not solved.')
        pass_check = False
        # return False

    return pass_check
    # return True
    # if not is_syn_timeout and not is_syn_third_pkt_timeout and not is_timeout and not is_fin_timeout:
    #     return True
    # return False


if __name__ == "__main__":
    with open(get_filename()) as f:
        all_lines = f.readlines()

    check_result = check_timeout(all_lines)
    print("Check result: ", check_result)
