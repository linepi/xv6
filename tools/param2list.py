import json
import sys

file_list = ""
file_list = file_list.split()
json.dump(file_list, sys.stdout)