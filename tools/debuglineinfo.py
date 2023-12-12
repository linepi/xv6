#!/Users/wu/anaconda3/bin/python
import sys

inputs = sys.stdin.read()
lines = inputs.split('\n')

for line in lines:
	if '.c' not in line and '.h' not in line:
		continue
	tmp = line.split()
	if len(tmp) < 3 or tmp[1] == '-': 
		continue
	filename, linenumber, addr = tmp[0], tmp[1], tmp[2]
	print(addr, linenumber, filename)
