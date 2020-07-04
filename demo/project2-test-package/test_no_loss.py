#!/usr/bin/env python3

"""
Author: Qianru Li (qianruli@ucla.edu), Zhiyi Zhang (zhiyi@cs.ucla.edu), Zhehui Zhang (zhehui@cs.ucla.edu)
For the purpose of CS118 2020 Spring Project 2 demo: no packet loss
Usage: ./test_no_loss.py output_file or python3 test_no_loss.py output_file
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

def check_format(all_lines) -> bool:
    handshake_case1 = False
    handshake_case2 = False
    handshake_case3 = False
    fin_case1 = False
    fin_case2_ack = False
    fin_case2 = False
    global my_isn, server_isn, my_fin, server_fin

    # err_lst = []
    pass_check = True

    send_sn_lst = []
    recv_ack_lst = []
    for lino, line in enumerate(all_lines):
        if len(line) == 0:
            continue
        result = output_pattern.match(line)
        result = result.groupdict()

        # SEND SYN, get ISN
        # SEND 2254 0 SYN
        # Test case: Client initiates three-way handshake by sending a SYN packet with correct values in its header
        if result['event'] == 'SEND' and result['syn_flag'] is not None and not result['ack_flag']:
            my_isn = int(result['seq'])
            handshake_case1 = True
            print('[Info] Client ISN: ', my_isn)

        # RECV SYN ACK, get ISN
        # Test case: Server responses with SYN-ACK packet
        elif result['event'] == 'RECV' and result['syn_flag'] is not None and result['ack_flag'] is not None:
            server_isn = int(result['seq'])
            handshake_case2 = True
            print('[Info] Server ISN: ', server_isn)
            recv_ack_lst.append(int(result['ack']))

            if not handshake_case1:
                print('[Error] No SYN before SYN-ACK: line ', lino+1, line)
                pass_check = False
                continue
                # return False

        # SEND FIN
        # Test case: Client sends a FIN packet after transmitting a file
        elif result['event'] == 'SEND' and result['fin_flag'] is not None:
            my_fin = int(result['seq'])
            send_sn_lst.append(int(result['seq']))
            fin_case1 = True

        # RECV FIN
        # Test case: Server replies with a FIN packet after reciving the FIN
        # elif fin_case1 and result['event'] == 'RECV' and result['fin_flag'] is not None:
        elif result['event'] == 'RECV' and result['fin_flag'] is not None:
            server_fin = int(result['seq'])
            fin_case2 = True
            if (server_isn + 1) % 25601 != server_fin:
                print('[Error] Wrong server FIN seq. Expected:', (server_isn + 1) % 25601, 'line', lino+1, line)
                pass_check = False
                continue 

        # SEND ACK (for SYN-ACK and FIN)
        # Test case: third handshake after sending SYN and receiving SYN-ACK
        elif result['event'] == 'SEND' and result['ack_flag']:
            if fin_case1 and fin_case2: # FIN-ACK
                fin_case2_ack = True
                send_sn_lst.append(int(result['seq']))
                ack = int(result['ack'])
                if (server_fin + 1) % 25601 != ack:
                    print('[Error] Wrong client FIN-ACK ack. Expected:', (server_fin + 1) % 25601, 'line', lino+1, line)
                    pass_check = False
                    continue
                    # return False

            # elif fin_case1 or fin_case2:
            #     print('[Error] No client FIN or server FIN before: line', lino+1,  line)
            #     pass_check = False
            #     continue
                # return False

            elif not fin_case1 and not fin_case2: # SYN-ACK
                seq = int(result['seq'])
                send_sn_lst.append(seq)
                handshake_case3 = True
                if (my_isn + 1) % 25601 != seq:
                    # print('Wrong ack in 3rd handshake. expected %d, actual: %d'%((my_isn + 1) % 25601, seq))
                    print('[Error] Wrong seq in 3rd handshake. Expected:', (my_isn + 1) % 25601, 'line', lino+1, line)
                    pass_check = False
                    continue
                    # return False

            # if not (handshake_case1 and handshake_case2):
            #     print('[Error] No SYN or SYN-ACK before: line', lino+1, line)
            #     pass_check = False
            #     continue
                # return False

        # SEND DATA
        elif result['event'] == 'SEND':
            send_sn_lst.append(int(result['seq']))
            # if not (handshake_case1 and handshake_case2 and handshake_case3):
            #     print('[Error] Three-way handshake not completed before. line', lino+1, line)
            #     pass_check = False
            #     continue
                # return False

        # RECV ACK
        elif result['event'] == 'RECV' and result['ack_flag']:
            recv_ack_lst.append(int(result['ack']))

    if not (handshake_case1 and handshake_case2 and handshake_case3) :
        print('[Error] Three-way handshake not completed.')
        pass_check = False
        # return False

    if not (fin_case1 and fin_case2 and fin_case2_ack):
        print('[Error] Four-way FIN not completed.')
        pass_check = False
        # return False

    for index, ack in enumerate(recv_ack_lst):
        if index >= len(send_sn_lst):
            print('[Error] Mismatch seq count and ack count', len(recv_ack_lst), len(send_sn_lst))
            pass_check = False
            break
            # return False
        if ack != send_sn_lst[index]:
            print('[Error] Mismatch seq and ack', send_sn_lst[index], ack)
            pass_check = False
            # return False

    # return True
    return pass_check


if __name__ == "__main__":
    with open(get_filename()) as f:
        all_lines = f.readlines()

    check_result = check_format(all_lines)
    print("Check result: ", check_result)
