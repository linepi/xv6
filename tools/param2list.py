import json
import sys

file_list = "build/fs.img README build/user/_trace build/user/_ls build/user/_init build/user/_lazytest build/user/_rm build/user/_cat build/user/_wc build/user/_grind build/user/_zombie build/user/_mkdir build/user/_sleep build/user/_kill build/user/_cowtest build/user/_sh build/user/_xargs build/user/_test build/user/_ln build/user/_find build/user/_grep build/user/_stressfs build/user/_usertests build/user/_echo"
file_list = file_list.split()
json.dump(file_list, sys.stdout)