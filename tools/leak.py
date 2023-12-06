content = """
kalloc: 0x87661000
kalloc: 0x87664000
kalloc: 0x87675000
kalloc: 0x87674000
kalloc: 0x87673000
kalloc: 0x87671000
kalloc: 0x87670000
kalloc: 0x87662000
kalloc: 0x87656000
kalloc: 0x87655000
kalloc: 0x87654000
kalloc: 0x87653000
kalloc: 0x87652000
kalloc: 0x87651000
kalloc: 0x87650000
kalloc: 0x8764f000
kalloc: 0x8764e000
kalloc: 0x8764d000
kalloc: 0x8764c000
kalloc: 0x8764b000
kalloc: 0x8764a000
kalloc: 0x87649000
kalloc: 0x87648000
kalloc: 0x87647000
kalloc: 0x87646000
kalloc: 0x87645000
kalloc: 0x87644000
kalloc: 0x87643000
kalloc: 0x87642000
kalloc: 0x87641000
kfree: 0x87661000
kfree: 0x87664000
kfree: 0x87654000
kfree: 0x87655000
kfree: 0x87653000
kfree: 0x8764f000
kfree: 0x8764e000
kfree: 0x8764d000
kfree: 0x8764c000
kfree: 0x8764b000
kfree: 0x8764a000
kfree: 0x87649000
kfree: 0x87648000
kfree: 0x87647000
kfree: 0x87646000
kfree: 0x87645000
kfree: 0x87644000
kfree: 0x87643000
kfree: 0x87642000
kfree: 0x87641000
kfree: 0x87670000
kfree: 0x87671000
kfree: 0x87651000
kfree: 0x87652000
kfree: 0x87673000
kfree: 0x87674000
kfree: 0x87675000
kfree: 0x87662000
kfree: 0x87650000
"""

a = content.split('\n')[1:-1]

kalloc = []
for pat in a:
	res = pat.split(' ')
	if len(res) == 3:
		res = res[1:]

	if res[0][1] == 'f':
		kalloc.remove(res[1])
	elif res[0][1] == 'a':
		kalloc.append(res[1])
	else:
		print(f'unknown: {res}')

for mem in kalloc:
	print(f'leak {mem}')