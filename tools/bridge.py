import argparse
import sys

# In/out from perspective of the C++ code
WLED_FIFO_IN = "/var/chip/wled-fifo-in"
WLED_FIFO_OUT = "/var/chip/wled-fifo-out"

def main() -> None:
    parser = argparse.ArgumentParser(description="Add/remove devices to bridge")
    
    parser.add_argument("action", choices=["add", "remove"])
    parser.add_argument("device", help="ip or hostname", type=str)

    args = parser.parse_args()
    
    if args.action == "add":
        action = "1"
    if args.action == "remove":
        action = "2"

    with open(WLED_FIFO_IN, "w") as fp:
        fp.write(action)
        fp.write(args.device)
    with open(WLED_FIFO_OUT, "r") as fp:
        data = fp.read()
        try:
            if int(data) != 0:
                print(f"Could not {args.action} device!", file=sys.stderr)
                exit(1)
        except ValueError:
            print("Did not get a response from bridge!")
            exit(2)

if __name__ == "__main__":
    main()
