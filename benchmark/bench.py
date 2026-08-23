import sys, os
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("-exe", metavar="exe", help="Path to q4b.exe", required=True)
# parser.add_argument("-o", metavar="output_dir", help="Output folder", required=False)
parser.add_argument("input_file", help="The file to test")
parser.add_argument("run_count", type=int, help="Number of times to run the benchmark")
parser.add_argument("-warm", help="Specify this to run one extra run, throwing out the first result, as it's probably a cold run which can impact performance.", action="store_true", required=False)
parser.add_argument("-p", metavar="", help="How to output the data. Options are \"print\" (default), \"csv\", and \"plot\" (requires matplotlib).", required=False)
parser.add_argument("-pout", metavar="name", help="Output file name. Default is \"output\", and the extension is automatically added.", default="output", required=False)
parser.add_argument("formats", nargs="*", help="List of formats to run. Needs the scheme and compression level. (examples: lz4 9 zstd 3-19 brotli 0,11)")
args = parser.parse_args()

if len(args.formats) < 2:
	sys.exit("Must specify at least one scheme and compression level")
if args.run_count <= 0:
	sys.exit()

EXECUTABLE = args.exe
# OUTPUT_DIR = args.o
# if len(OUTPUT_DIR) == 0:
# 	OUTPUT_DIR = "."
# if OUTPUT_DIR[-1] != '/':
#	# TODO: from pathlib import Path
# 	OUTPUT_DIR += "/"
FILE = args.input_file
RUN_COUNT = args.run_count
SKIP_FIRST_RUN = args.warm

SCHEMES = []
for i in range(int(len(args.formats)/2)):
	levels = args.formats[i*2+1].split(',')
	for l in levels:
		if l.count('-') == 0:
			SCHEMES.append((args.formats[i*2], l))
		elif l.count('-') == 1:
			begin = int(l.split('-')[0])
			end   = int(l.split('-')[1])
			for value in range(begin, end+1):
				SCHEMES.append((args.formats[i*2], str(value)))
		else:
			sys.exit("Incorrectly formatted compression level")

data = []
for scheme, level in SCHEMES:
	times = []
	original_size = 0
	compressed_size = 0

	for _ in range(RUN_COUNT + int(SKIP_FIRST_RUN)):
		# Run:

		result = subprocess.run([EXECUTABLE, "compress", FILE, scheme, level, "--bench"], capture_output=True, text=True)
		result_list = result.stdout.split('\n')
		if len(result_list) < 3:
			sys.exit("ERROR: Invalid output")

		time_output            = result_list[0]
		original_size_output   = result_list[1]
		compressed_size_output = result_list[2]

		# Calculate time taken:

		valid_output = False
		if len(time_output) == 2:
			if time_output[1] == 's' and time_output[0].isdecimal():
				valid_output = True
		elif len(time_output) > 2:
			if time_output[-1] == 's' and (time_output[-2] in ['n', 'u', 'm'] or time_output[-2].isdecimal()) and time_output[:-2].isdecimal():
				valid_output = True
		# Regex version: "[0-9]+[num]?s"
		if not valid_output:
			sys.exit("ERROR: Bad time taken")

		seconds = 0
		if time_output[-2] == 'n':
			seconds = int(time_output[:-2]) / 1E9
		elif time_output[-2] == 'u':
			seconds = int(time_output[:-2]) / 1E6
		elif time_output[-2] == 'm':
			seconds = int(time_output[:-2]) / 1E3
		else:
			seconds = int(time_output[:-1])

		times.append(seconds)

		# Verify size:

		if not original_size_output.isdecimal():
			sys.exit("ERROR: Bad original size")
		if not compressed_size_output.isdecimal():
			sys.exit("ERROR: Bad compressed size")

		if original_size == 0 and compressed_size == 0:
			# Neither are set, meaning it's the first run (or an error when compressing an empty file)
			original_size = int(original_size_output)
			compressed_size = int(compressed_size_output)
		else:
			# At least one is set
			if original_size != int(original_size_output):
				sys.exit("ERROR: Original size has changed between runs")
			if compressed_size != int(compressed_size_output):
				sys.exit("ERROR: Compressed size has changed between runs")

	if SKIP_FIRST_RUN:
		times = times[1:]

	avg_time = sum(times) / RUN_COUNT
	ratio = (original_size / compressed_size) if compressed_size else 0
	data.append([scheme + " " + level, avg_time, ratio])

if args.p == "csv":
	import csv
	with open(args.pout + ".csv", newline='\n', mode='w') as csvfile:
		writer = csv.writer(csvfile)
		writer.writerow(["Scheme and Level", "Time (s)", "Compression Ratio"])
		for run in data:
			writer.writerow(run)

elif args.p == "plot":
	try:
		import matplotlib.pyplot as plt
	except ImportError:
		sys.exit("Could not import matplotlib")

	#TODO
	pass

else:
	for run in data:
		for i in range(1, len(run)):
			run[i] = round(run[i], 4)
	print("\n".join(map(str, data)))
