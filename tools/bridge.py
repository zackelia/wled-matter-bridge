#!../venv/bin/python
import argparse
import subprocess
import sys
from pathlib import Path

# In/out from perspective of the C++ code
WLED_FIFO_IN = "/var/chip/wled-fifo-in"
WLED_FIFO_OUT = "/var/chip/wled-fifo-out"

def main() -> None:
    parser = argparse.ArgumentParser(description="Add/remove devices to bridge")

    subparsers = parser.add_subparsers(dest="action")
    add_parser = subparsers.add_parser("add")
    remove_parser = subparsers.add_parser("remove")
    qr_parser = subparsers.add_parser("qr")

    for subparser in [add_parser, remove_parser]:
        subparser.add_argument("device", help="ip or hostname", type=str)

    args = parser.parse_args()

    if args.action == "add":
        action = "1"
    if args.action == "remove":
        action = "2"
    if args.action == "qr":
        action = "3"

    with open(WLED_FIFO_IN, "w") as fp:
        fp.write(action)
        if args.action in ["add", "remove"]:
            fp.write(args.device)
        else:
            fp.write("1")
    with open(WLED_FIFO_OUT, "r") as fp:
        data = fp.read()
        if args.action in ["add", "remove"]:
            try:
                if int(data) != 0:
                    print(f"Could not {args.action} device!", file=sys.stderr)
                    exit(1)
            except ValueError:
                print("Did not get a response from bridge!")
                exit(2)
        if args.action in ["qr"]:
            if not data:
                print("Could not get QR code from bridge")
                exit(3)
            print(data)
            result = subprocess.run([Path(sys.executable).parent / "qr", "--ascii", data], stdout=subprocess.PIPE)
            print(result.stdout.decode())
            

if __name__ == "__main__":
    main()
