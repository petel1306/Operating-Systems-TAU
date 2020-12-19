import subprocess
import random
import string
from colorama import Fore, Back, Style 
import json
import errno
from collections import namedtuple
import functools


SENDER_EXECUTABLE_PATH = "./message_sender.o"
READER_EXECUTABLE_PATH = "./message_reader.o"


FILE_NAMES = {}


RunResult = namedtuple('RunResult', ['returncode', 'stdout', 'stderr'])


def execute(command, exit_on_non_zero=True):
    result = subprocess.run(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if exit_on_non_zero and result.returncode != 0:
        print(f"Error executing command '{command}'! Existing...\n")
        print(f"return code = {result.returncode}")
        print("stdout")
        print(result.stdout.decode('utf-8'))
        print("stderr")
        print(result.stderr.decode('utf-8'))
        exit(-1)
    return RunResult(result.returncode, result.stdout.decode('utf-8'), result.stderr.decode('utf-8'))
    # return {
    #     'returncode': result.returncode,
    #     'stdout': result.stdout.decode('utf-8'),
    #     'stderr': result.stderr.decode('utf-8'),
    # }


def delete_all_files(ignore=None):
    global FILE_NAMES

    ignore = ignore or []

    for filename in FILE_NAMES:
        if filename not in ignore:
            execute(f'sudo rm {filename}')
            

def create_slot_file(template, minor_num):
    global FILE_NAMES

    filename = f"{template}{minor_num}"
    if filename in FILE_NAMES:
        print("THIS IS WRONG! Contact the developer of this file and tell him he made a mistake.")
        delete_all_files()
        exit(-1)
    execute(f"test ! -f {filename} && sudo mknod {filename} c 240 {minor_num} || echo '' > /dev/null")
    execute(f"sudo chmod o+rw {filename}")
    FILE_NAMES[filename] = {}
    return filename


def write_to_file(filename, channel_id, msg, **kwargs):
    global FILE_NAMES

    # Update local copy of data for checks
    FILE_NAMES[filename][channel_id] = msg
    # print(f"Write({filename}, {channel_id}, value={msg})")

    # Execute command to write the data
    return execute(f"{SENDER_EXECUTABLE_PATH} {filename} {channel_id} '{msg}'", **kwargs)


def assert_equal(result, expected, extra_info={}):
    if result != expected:
        print("Error, assertion failed.")
        print(f"Expected: {Fore.RED}{result}{Style.RESET_ALL}")
        print(f"to equal: {Fore.GREEN}{expected}{Style.RESET_ALL}")
        extra_info_and_exit(extra_info)


def assert_contains(containing, contained, extra_info={}):
    if contained not in containing:
        print("Error, assertion failed.")
        print(f"Expected: {Back.YELLOW}{Fore.BLACK}{contained}{Style.RESET_ALL}")
        print(f"to be contained in: {Back.YELLOW}{Fore.BLACK}{containing}{Style.RESET_ALL}")
        extra_info_and_exit(extra_info)


def extra_info_and_exit(extra_info):
    print(f"Extra information:")
    print(json.dumps(extra_info, indent=4))
    delete_all_files()
    exit(-1)


def read_from_file(filename, channel_id):
    return execute(f"{READER_EXECUTABLE_PATH} {filename} {channel_id}", exit_on_non_zero=False)


def assert_read(filename, channel_id):
    global FILE_NAMES

    # print(f"Read({filename}, {channel_id})")

    result = read_from_file(filename, channel_id)
    expected = FILE_NAMES[filename].get(channel_id, "")

    if expected == "":
        # Can't assume file is emtpy
        pass
    else:
        assert_equal(result.stdout, expected, {
            'filename': filename,
            'channel_id': channel_id,
            'execute result': result,
            })


def get_random_value():
    length = random.randint(1, 16)
    letters = list(string.ascii_lowercase + string.ascii_uppercase + string.digits)
    return ''.join(random.choice(letters) for i in range(length))


def get_random_filename_and_channel_id(truely_random_channel_id_frac=0.05):
    filename = random.choice(list(FILE_NAMES.keys()))
    channel_id_list = list(FILE_NAMES[filename].keys())
    if random.random() < 0.05 or len(channel_id_list) == 0:
        channel_id = random.randrange(2 ** 20, (2 ** 20) + 20)
    else:
        channel_id = random.choice(channel_id_list)
    
    return filename, channel_id


def assert_read_random():
    global FILE_NAMES

    assert_read(*get_random_filename_and_channel_id())


def write_random():
    global FILE_NAMES

    write_to_file(*get_random_filename_and_channel_id(), get_random_value())


def perform_random_operation():
    options = [
        assert_read_random,
        write_random,
    ]

    op = random.choice(options)

    op()


def remove_all_files_with_prefix(folder, prefix):
    execute(f"sudo find {folder} -name \"{prefix}*\" -delete")


def test_wrapper(f):
    @functools.wraps(f)
    def wrapper(*args, **kwargs):
        print(f"  {f.__name__}(", end='')
        for arg in args:
            print(arg, end='')
        for key, value in kwargs.items():
            print(f"{key}={value}", end='')
        print(')')
        f(*args, **kwargs)
        print("\033[F\033[0G✅")
    return wrapper


@test_wrapper
def test_random_operations(amount):
    for _ in range(amount):
        perform_random_operation()


@test_wrapper
def test_ioctl_zero_fails():
    global FILE_NAMES

    for filename in FILE_NAMES:
        result = read_from_file(filename, 0)
        assert_contains(result.stderr, 'Invalid argument', {
            'filename': filename
        })


@test_wrapper
def test_empty_write_fails():
    global FILE_NAMES

    for filename in FILE_NAMES:
        result = write_to_file(filename, 1, "", exit_on_non_zero=False)
        assert_contains(result.stderr, "Message too long", {'filename': filename})


@test_wrapper
def test_long_write_fails():
    global FILE_NAMES

    size = 130

    filename = next(iter(FILE_NAMES.keys()))
    result = write_to_file(filename, 1, "a" * size, exit_on_non_zero=False)
    assert_contains(result.stderr, "Message too long", {'filename': filename, 'message_length': size, 'execution result': result})


@test_wrapper
def test_can_print_not_to_large():
    global FILE_NAMES

    filename = next(iter(FILE_NAMES.keys()))
    for size in range(1, 129):
        result = write_to_file(filename, 1, "a" * size, exit_on_non_zero=False)
        assert_equal(result.returncode, 0, {
            'filename': filename,
            'message': "a" * size,
            'result': result,
        })
        assert_read(filename, 1)


if __name__ == '__main__':
    filename_template = '/dev/char_dev_test'

    remove_all_files_with_prefix("/dev", "char_dev_test")

    for i in range(2):
        create_slot_file(filename_template, i)

    test_ioctl_zero_fails()
    test_long_write_fails()
    test_empty_write_fails()
    test_can_print_not_to_large()

    test_random_operations(amount=1000)

    delete_all_files()
