import sys, os
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("-exe", metavar="exe", help="Path to q4b.exe", required=True)
parser.add_argument("-o", metavar="output_dir", help="Output folder", required=True)
parser.add_argument("input_file", help="The file to test")
parser.add_argument("run_count", type=int, help="Number of times to run the benchmark")
parser.add_argument("-warm", help="Specify this to run one extra run, throwing out the first result, as it's probably a cold run which can impact performance.", action="store_true", required=False)
# parser.add_argument("-p", help="How to output the data. Options are \"print\" (default), \"csv\", and \"plot\" (requires matplotlib)", required=False)
parser.add_argument("formats", nargs="*", help="List of formats to run. Needs the scheme and compression level. (examples: lz4 3 zstd 19 brotli 10)")
args = parser.parse_args()

if len(args.formats) < 2:
	sys.exit("Must specify at least one scheme and compression level")
if args.run_count <= 0:
	sys.exit()

EXECUTABLE = args.exe
OUTPUT_DIR = args.o
if len(OUTPUT_DIR) == 0:
	OUTPUT_DIR = "."
if OUTPUT_DIR[-1] != '/':
	OUTPUT_DIR += "/"
FILE = args.input_file
RUN_COUNT = args.run_count
SKIP_FIRST_RUN = args.warm

SCHEMES = []
for i in range(int(len(args.formats)/2)):
	SCHEMES.append((args.formats[i*2], args.formats[i*2+1]))

data = []
for scheme, level in SCHEMES:
	times = []
	for _ in range(RUN_COUNT + int(SKIP_FIRST_RUN)):
		result = subprocess.run([EXECUTABLE, "compress", FILE, scheme, level, "-o", OUTPUT_DIR, "--bench"], capture_output=True, text=True)

		valid_output = False
		if len(result.stdout) == 2:
			if result.stdout[1] == 's' and result.stdout[0].isdecimal():
				valid_output = True
		elif len(result.stdout) > 2:
			if result.stdout[-1] == 's' and (result.stdout[-2] in ['n', 'u', 'm'] or result.stdout[-2].isdecimal()) and result.stdout[:-2].isdecimal():
				valid_output = True
		# Regex version: "[0-9]+[num]?s"
		if not valid_output:
			sys.exit("ERROR: Could not compress file")

		seconds = 0
		if result.stdout[-2] == 'n':
			seconds = int(result.stdout[:-2]) / 1E9
		elif result.stdout[-2] == 'u':
			seconds = int(result.stdout[:-2]) / 1E6
		elif result.stdout[-2] == 'm':
			seconds = int(result.stdout[:-2]) / 1E3
		else:
			seconds = int(result.stdout[:-1])

		times.append(seconds)

	if SKIP_FIRST_RUN:
		times = times[1:]
	# data.append((round(sum(times) / RUN_COUNT, 4), round(os.path.getsize(FILE) / os.path.getsize(OUTPUT_DIR + FILE + "." + scheme), 4)))
	data.append(round(sum(times) / RUN_COUNT, 4))

print(data)
