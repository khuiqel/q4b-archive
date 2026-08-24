import sys
import argparse
import subprocess

### ARGPARSE ###

parser = argparse.ArgumentParser()
parser.add_argument("exe", help="Path to q4b.exe")
parser.add_argument("-o", metavar="dir", help="Output folder for the compressed file, to test decompression on; if not specified, then decompression will not be tested", required=False)

parser.add_argument("input_file", help="The file to test")
parser.add_argument("run_count", type=int, help="Number of times to run the benchmark")
parser.add_argument("-warm", help="Specify this to run one extra run, throwing out the first result, as it's probably a cold run which can impact performance.", action="store_true", required=False)
parser.add_argument("formats", nargs="*", help="List of formats to run when compressing. Needs the scheme and compression level. (examples: lz4 9 zstd 3-19 brotli 0,11)")

parser.add_argument("-p", metavar="type", help="How to output the data. Options are \"print\" (default), \"csv\", and \"plot\" (requires matplotlib).", required=False)
parser.add_argument("-pout", metavar="name", help="Output file name. Default is \"output\", and the extension is automatically added. (Not used with \"-p print\".)", default="output", required=False)
args = parser.parse_args()

# Data output:
# C:  ["Scheme and Level", "Compression Ratio", "Compression Time (s)"]
# CD: ["Scheme and Level", "Compression Ratio", "Compression Time (s), "Decompression Time (s)"]

DO_DECOMPRESSION = args.o is not None
EXECUTABLE = args.exe
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

if len(SCHEMES) < 1:
	sys.exit("Must specify at least one scheme and compression level")
if args.run_count <= 0:
	sys.exit()

### EXECUTE ###

data = []
for scheme, level in SCHEMES:
	ctimes = []
	dtimes = []
	compressed_filename = None

	original_size = 0
	compressed_size = 0
	decompressed_size = 0

	for i in range(RUN_COUNT + int(SKIP_FIRST_RUN)):
		# Run:

		COMMAND_ARGS = [EXECUTABLE, "compress", FILE, scheme, level, "--bench"]
		if i == 0 and DO_DECOMPRESSION:
			COMMAND_ARGS.extend(["-o", args.o])

		result = subprocess.run(COMMAND_ARGS, capture_output=True, text=True)
		result_list = result.stdout.split('\n')
		if len(result_list) < 4:
			sys.exit("ERROR: Invalid output")

		time_output            = result_list[0]
		original_size_output   = result_list[1]
		compressed_size_output = result_list[2]
		filename_output        = result_list[3]

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

		ctimes.append(seconds)

		# Verify size:

		if not original_size_output.isdecimal():
			sys.exit("ERROR: Bad original size")
		if not compressed_size_output.isdecimal():
			sys.exit("ERROR: Bad compressed size")

		if i == 0:
			original_size = int(original_size_output)
			compressed_size = int(compressed_size_output)
			compressed_filename = filename_output
		else:
			# Verify the files are the same as last time
			if original_size != int(original_size_output):
				sys.exit("ERROR: Original size has changed between runs")
			if compressed_size != int(compressed_size_output):
				sys.exit("ERROR: Compressed size has changed between runs")

	for i in range(RUN_COUNT + int(SKIP_FIRST_RUN)):
		if not DO_DECOMPRESSION:
			dtimes.append(0)
			continue

		# Run:

		result = subprocess.run([EXECUTABLE, "decompress", compressed_filename, scheme, "--bench"], capture_output=True, text=True)
		# Adding scheme is unnecessary but theoretically guarantees testing the same scheme
		result_list = result.stdout.split('\n')
		if len(result_list) < 4:
			sys.exit("ERROR: Invalid output (decompress)")

		time_output              = result_list[0]
		compressed_size_output   = result_list[1]
		decompressed_size_output = result_list[2]
		scheme_output            = result_list[3]

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

		dtimes.append(seconds)

		# Verify size:

		if not compressed_size_output.isdecimal():
			sys.exit("ERROR: Bad compressed size")
		if not decompressed_size_output.isdecimal():
			sys.exit("ERROR: Bad decompressed size")
		if compressed_size != int(compressed_size_output):
			sys.exit("ERROR: Compressed size has changed between runs")

		if i == 0:
			decompressed_size = int(decompressed_size_output)
			if decompressed_size != original_size:
				sys.exit("ERROR: Original size does not match decompressed size")
		else:
			if decompressed_size != int(decompressed_size_output):
				sys.exit("ERROR: Decompressed size has changed between runs")

	if SKIP_FIRST_RUN:
		ctimes = ctimes[1:]
		dtimes = dtimes[1:]

	ratio = (original_size / compressed_size) if compressed_size else 0
	avg_ctime = sum(ctimes) / RUN_COUNT
	avg_dtime = sum(dtimes) / RUN_COUNT
	data.append([scheme + " " + level, ratio, avg_ctime] + ([avg_dtime] if DO_DECOMPRESSION else []))

### RESULTS ###

if args.p == "csv":
	import csv
	with open(args.pout + ".csv", newline='\n', mode='w') as csvfile:
		writer = csv.writer(csvfile)
		writer.writerow(["Scheme and Level", "Compression Ratio", "Compression Time (s)"] + (["Decompression Time (s)"] if DO_DECOMPRESSION else []))
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
