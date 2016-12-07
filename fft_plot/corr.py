import numpy as np
import matplotlib.pyplot as plt
import sys
#import sys from argv
if len(sys.argv) <2:
	print 'usage: python corr.py filename'
	sys.exit()
filename = sys.argv[1]
signal = []
data = []
freq = []
count = 0
def readfile(filename, count):
	input_file = open(filename)
	for line in input_file:
		line = line.split(',')
		if len(line)==9:
			#print line[6]
			temp_sig = line[0].split(':')
			temp_data = line[6].split(':')
			temp_freq = line[2].split(':')	
			if float(temp_freq[1].strip())< 2459:
				continue 
			if float(temp_freq[1].strip())> 2461:
				continue
			signal.append(temp_sig[1].strip())
			data.append(int(temp_data[1].strip()))
			freq.append(temp_freq[1].strip())
	 
readfile(filename, count)

#print count
#plt.scatter(freq, signal)
index=range(0, len(signal))
#plt.plot(freq)
plt.hist(np.asarray(signal))
#plt.ylim(-160, -20)
plt.xlabel("Index")
plt.ylabel("Signal (dBm)")
plt.show()
