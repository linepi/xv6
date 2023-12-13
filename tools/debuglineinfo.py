#!/Users/wu/anaconda3/bin/python
import sys

inputs = sys.stdin.read()
lines = inputs.split('\n')

lastaddr = 0
line_len = 40

for line in lines:
	if '.c' not in line and '.h' not in line:
		continue
	tmp = line.split()
	if len(tmp) < 3 or tmp[1] == '-': 
		continue
	filename, linenumber, addr = tmp[0], tmp[1], tmp[2]
	if addr != lastaddr:
		print("%s %s:%s" % (addr, filename, linenumber), end='')
		for i in range(line_len - len(addr + linenumber + filename) - 2):
			print(' ', end='')
		print()
	lastaddr = addr
