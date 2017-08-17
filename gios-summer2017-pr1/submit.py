import os
import argparse
from nelson.gtomscs import submit
# see https://github.com/udacity/nelson for nelson information (OMSCS specific)

def main():
    parser = argparse.ArgumentParser(description='Submits code to the Udacity site.')
    parser.add_argument('quiz', choices=['echo', 'transfer', 'gfclient', 'gfserver', 'gfclient_mt', 'gfserver_mt', 'readme'])

    args = parser.parse_args()

    path_map = {'echo': 'echo',
                'transfer': 'transfer',
                'gfclient': 'gflib',
                'gfserver': 'gflib',
                'gfclient_mt': 'mtgf',
                'gfserver_mt': 'mtgf',
                'readme': '.'}

    quiz_map = {'echo': 'pr1_echo_client_server',
                'transfer': 'pr1_transfer',
                'gfclient': 'pr1_gfclient',
                'gfserver': 'pr1_gfserver',
                'gfclient_mt': 'pr1_gfclient_mt',
                'gfserver_mt': 'pr1_gfserver_mt',
                'readme': 'pr1_readme'}

    files_map = {'pr1_echo_client_server': ['echoclient.c', 'echoserver.c'],
                 'pr1_transfer': ['transferclient.c', 'transferserver.c'],
                 'pr1_gfclient': ['gfclient.c'],
                 'pr1_gfserver': ['gfserver.c'],
                 'pr1_gfclient_mt': ['gfclient_download.c'],
                 'pr1_gfserver_mt': ['gfserver_main.c', 'handler.c'],
                 'pr1_readme': ['readme-student.md']}

    quiz = quiz_map[args.quiz]

    os.chdir(path_map[args.quiz])

    submit('cs8803-02', quiz, files_map[quiz])

if __name__ == '__main__':
    main()